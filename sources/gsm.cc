//#
//# FILE: gsm.cc  implementation of GSM (Stack machine)
//#
//# $Id$
//#


#include <assert.h>
#include "gambitio.h"


#include "gsm.h"


//--------------------------------------------------------------------
//              implementation of GSM (Stack machine)
//--------------------------------------------------------------------

GSM::GSM( int size )
{
#ifndef NDEBUG
  if( size <= 0 )
  {
    gerr << "GSM Error: illegal stack size specified during initialization\n";
    gerr << "           stack size requested: " << size << "\n";
  }
  assert( size > 0 );
#endif // NDEBUG

  _Stack         = new gGrowableStack< Portion* >( size );
  _CallFuncStack = new gGrowableStack< CallFuncObj* >( size ) ;
  _RefTable      = new RefHashTable;
  _FuncTable     = new FunctionHashTable;

  InitFunctions();  // This function is located in gsmfunc.cc
}


GSM::~GSM()
{
  Flush();
  delete _FuncTable;
  delete _RefTable;
  delete _CallFuncStack;
  delete _Stack;
}


int GSM::Depth( void ) const
{
  return _Stack->Depth();
}


int GSM::MaxDepth( void ) const
{
  return _Stack->MaxDepth();
}





//------------------------------------------------------------------------
//                           Push() functions
//------------------------------------------------------------------------

bool GSM::Push( const bool& data )
{
  Portion*  p;
  
  p = new bool_Portion( data );
  _Stack->Push( p );

  return true;
}


bool GSM::Push( const double& data )
{
  Portion*  p;
  
  p = new numerical_Portion<double>( data );
  _Stack->Push( p );

  return true;
}


bool GSM::Push( const gInteger& data )
{
  Portion*  p;

  p = new numerical_Portion<gInteger>( data );
  _Stack->Push( p );

  return true;
}


bool GSM::Push( const gRational& data )
{
  Portion*  p;

  p = new numerical_Portion<gRational>( data );
  _Stack->Push( p );

  return true;
}


bool GSM::Push( const gString& data )
{
  Portion*  p;

  p = new gString_Portion( data );
  _Stack->Push( p );

  return true;
}





// This function is only temporarily here for testing reasons
/*
bool GSM::GenerateNfg( const double& data )
{
  Nfg_Portion<double>* p;
  p = new Nfg_Portion( *(new Nfg) );
  p->Value().value = data;
  p->Temporary() = false;
  _Stack->Push( p );
  return true;
}
*/




bool GSM::PushList( const int num_of_elements )
{ 
  int            i;
  Portion*       p;
  List_Portion*  list;
  bool           result = true;

#ifndef NDEBUG
  if( num_of_elements <= 0 )
  {
    gerr << "GSM Error: illegal number of elements requested to PushList()\n";
    gerr << "           elements requested: " << num_of_elements << "\n";
  }
  assert( num_of_elements > 0 );

  if( num_of_elements > _Stack->Depth() )
  {
    gerr << "GSM Error: not enough elements in GSM to PushList()\n";
    gerr << "           elements requested: " << num_of_elements << "\n";
    gerr << "           elements available: " << _Stack->Depth() << "\n";
  }
  assert( num_of_elements <= _Stack->Depth() );
#endif // NDEBUG

  list = new List_Portion;
  for( i = 1; i <= num_of_elements; i++ )
  {
    p = _Stack->Pop();
    if( p->Type() == porREFERENCE )
      p = _ResolveRef( (Reference_Portion*) p );
    if( list->Insert( p, 1 ) == 0 )
    {
      result = false;
    }
  }
  _Stack->Push( list );

  return result;
}



//---------------------------------------------------------------------
//     Reference related functions: PushRef(), Assign(), UnAssign()
//---------------------------------------------------------------------

bool GSM::PushRef( const gString& ref )
{
  Portion*  p;
  
  p = new Reference_Portion( ref );
  _Stack->Push( p );

  return true;
}


bool GSM::PushRef( const gString& ref, const gString& subref )
{
  Portion*  p;
  
  p = new Reference_Portion( ref, subref );
  _Stack->Push( p );

  return true;
}


bool GSM::Assign( void )
{
  Portion*  p2_copy;
  Portion*  p2;
  Portion*  p1;
  Portion*  primary_ref;
  gString   p1_subvalue;
  bool      result = true;

#ifndef NDEBUG
  if( _Stack->Depth() < 2 )
  {
    gerr << "GSM Error: not enough operands to execute Assign()\n";
  }
  assert( _Stack->Depth() >= 2 );
#endif // NDEBUG

  p2 = _Stack->Pop();
  p1 = _Stack->Pop();

  if ( p1->Type() == porREFERENCE )
  {
    p1_subvalue = ( (Reference_Portion*) p1 )->SubValue();

    if( p1_subvalue == "" )
    {
      if( p2->Type() == porREFERENCE )
      {
	p2 = _ResolveRef( (Reference_Portion*) p2 );
	p2_copy = p2->Copy();
	p2_copy->MakeCopyOfData( p2 );
      }
      else
      {
	p2_copy = p2->Copy();
      }
      p2_copy->Temporary() = false;
      p2->Temporary() = true;
      _RefTable->Define( ( (Reference_Portion*) p1 )->Value(), p2_copy );
      delete p2;
      
      p1 = _ResolveRef( (Reference_Portion*) p1 );
      _Stack->Push( p1 );
    }

    else // ( p1_subvalue != "" )
    {
      primary_ref = _ResolvePrimaryRefOnly( (Reference_Portion*) p1 );

      if( primary_ref->Type() & porALLOWS_SUBVARIABLES )
      {
	if( p2->Type() == porREFERENCE )
	{
	  p2 = _ResolveRef( (Reference_Portion*) p2 );
	  p2_copy = p2->Copy();
	  p2_copy->MakeCopyOfData( p2 );
	}
	else
	{
	  p2_copy = p2->Copy();
	}
	p2_copy->Temporary() = false;
	p2->Temporary() = true;

	switch( primary_ref->Type() )
	{
	case porNFG_DOUBLE:
	  ( (Nfg_Portion<double>*) primary_ref )->
	    Assign( p1_subvalue, p2_copy );
	  break;
	case porNFG_RATIONAL:
	  ( (Nfg_Portion<gRational>*) primary_ref )->
	    Assign( p1_subvalue, p2_copy );
	  break;
	case porLIST:
	  ( (List_Portion*) primary_ref )->
	    SetSubscript( atoi( p1_subvalue), p2_copy );
	  break;
	  
	default:
	  gerr << "GSM Error: unknown type supports subvariables\n";
	  assert(0);
	}

	delete p2;
	p1 = _ResolveRef( (Reference_Portion*) p1 );
      }
      else
      {
	gerr << "GSM Error: attempted to assign a sub-reference to a type\n";
	gerr << "           that doesn't support such structures\n";
	if( primary_ref->Type() == porERROR )
	{
	  delete primary_ref;
	}
	delete p2;
	delete p1;
	p1 = new Error_Portion;
      }
      _Stack->Push( p1 );
    }
  }
  else // ( p1->Type() != porREFERENCE )
  {
    gerr << "GSM Error: no reference found to be assigned\n";
    delete p1;
    delete p2;
    _Stack->Push( new Error_Portion );
    result = false;
  }
  return result;
}


bool GSM::UnAssign( void )
{
  Portion*  p1;
  Portion*  primary_ref;
  gString   ref;
  gString   p1_subvalue;
  bool      result = true;

#ifndef NDEBUG
  if( _Stack->Depth() < 1 )
  {
    gerr << "GSM Error: not enough operands to execute Assign()\n";
  }
  assert( _Stack->Depth() >= 1 );
#endif // NDEBUG

  p1 = _Stack->Pop();

  if ( p1->Type() == porREFERENCE )
  {
    ref = ( (Reference_Portion*) p1 )->Value();
    p1_subvalue = ( (Reference_Portion*) p1 )->SubValue();
    if( p1_subvalue == "" )
    {
      if( _RefTable->IsDefined( ref ) )
      {
	_RefTable->Remove( ref );
      }
    }

    else // ( p1_subvalue != "" )
    {
      primary_ref = _ResolvePrimaryRefOnly( (Reference_Portion*) p1 );

      if( primary_ref->Type() & porALLOWS_SUBVARIABLES )
      {
	switch( primary_ref->Type() )
	{
	case porNFG_DOUBLE:
	  ( (Nfg_Portion<double>*) primary_ref )->UnAssign( p1_subvalue );
	  break;
	case porNFG_RATIONAL:
	  ( (Nfg_Portion<gRational>*) primary_ref )->UnAssign( p1_subvalue );
	  break;
	  
	default:
	  gerr << "GSM Error: unknown type supports subvariables\n";
	  assert(0);
	}
      }
      else
      {
	gerr << "GSM Error: attempted to unassign a sub-reference of a type\n";
	gerr << "           that doesn't support such structures\n";
	if( primary_ref->Type() == porERROR )
	{
	  delete primary_ref;
	}
	delete p1;
	p1 = new Error_Portion;
      }
    }
    delete p1;
  }
  else // ( p1->Type() != porREFERENCE )
  {
    gerr << "GSM Error: no reference found to be unassigned\n";
    result = false;
  }
  return result;
}




//---------------------------------------------------------------------
//                        _ResolveRef functions
//-----------------------------------------------------------------------

Portion* GSM::_ResolveRef( Reference_Portion* p )
{
  Portion*  result = 0;
  Portion*  temp;
  gString&  ref = p->Value();
  gString&  subvalue = p->SubValue();


  if( _RefTable->IsDefined( ref ) )
  {
    if( subvalue == "" )
    {
      result = (*_RefTable)( ref )->Copy();
    }
    else
    {
      result = (*_RefTable)( ref );
      switch( result->Type() )
      {
      case porNFG_DOUBLE:
	result = ((Nfg_Portion<double>*) result )->operator()( subvalue );
	break;
      case porNFG_RATIONAL:
	result = ((Nfg_Portion<gRational>*) result )->operator()( subvalue );
	break;
      case porLIST:
	result = ((List_Portion*) result )->GetSubscript( atoi( subvalue ) );
	break;

      default:
	gerr << "GSM Error: attempted to resolve a subvariable of a type\n";
	gerr << "           that does not support subvariables\n";
	result = new Error_Portion;
      }
      if( result != 0 )
      {
	if( result->Type() != porERROR )
	  result = result->Copy();
      }
      else
      {
	result = new Error_Portion;
      }
    }
  }
  else
  {
    gerr << "GSM Error: attempted to resolve an undefined reference\n";
    gerr << "           \"" << ref << "\"\n";
    result = new Error_Portion;
  }
  delete p;

  return result;
}


Portion* GSM::_ResolveRefWithoutError( Reference_Portion* p )
{
  Portion*  result = 0;
  Portion*  temp;
  gString&  ref = p->Value();
  gString&  subvalue = p->SubValue();

  if( _RefTable->IsDefined( ref ) )
  {
    if( subvalue == "" )
    {
      result = (*_RefTable)( ref )->Copy();
    }
    else
    {
      result = (*_RefTable)( ref );
      switch( result->Type() )
      {
      case porNFG_DOUBLE:
	if( ((Nfg_Portion<double>*) result )->IsDefined( subvalue ) )
	{
	  result = ((Nfg_Portion<double>*) result )->
	    operator()( subvalue )->Copy();
	}
	else
	{
	  result = 0;
	}
      case porNFG_RATIONAL:
	if( ((Nfg_Portion<gRational>*) result )->IsDefined( subvalue ) )
	{
	  result = ((Nfg_Portion<gRational>*) result )
	    ->operator()( subvalue )->Copy();
	}
	else
	{
	  result = 0;
	}
      case porLIST:
	((List_Portion*) result )->GetSubscript( atoi( subvalue ) );
	if( result != 0 )
	  result = result->Copy();
	break;

      default:
	gerr << "GSM Error: attempted to resolve the subvariable of a type\n";
	gerr << "           that does not support subvariables\n";
	result = new Error_Portion;
      }
    }
  }
  else
  {
    result = 0;
  }
  delete p;

  return result;
}



Portion* GSM::_ResolvePrimaryRefOnly( Reference_Portion* p )
{
  Portion*  result = 0;
  Portion*  temp;
  gString&  ref = p->Value();
  gString   subvalue;

  if( _RefTable->IsDefined( ref ) )
  {
    result = (*_RefTable)( ref );
  }
  else
  {
    gerr << "GSM Error: attempted to resolve an undefined reference\n";
    gerr << "           \"" << ref << "\"\n";
    result = new Error_Portion;
  }

  return result;
}



//------------------------------------------------------------------------
//                       binary operations
//------------------------------------------------------------------------


// Main dispatcher of built-in binary operations

// operations are dispatched to the appropriate Portion classes,
// except in cases where the return type is differen from parameter
// #1, in which case the operation implementation would be placed here
// labeled as SPECIAL CASE HANDLING

bool GSM::_BinaryOperation( OperationMode mode )
{
  Portion*   p2;
  Portion*   p1;
  Portion*   p;
  bool       result = true;

#ifndef NDEBUG
  if( _Stack->Depth() < 2 )
  {
    gerr << "GSM Error: not enough operands to perform binary operation\n";
    result = false;
  }
  assert( _Stack->Depth() >= 2 );
#endif // NDEBUG
  
  p2 = _Stack->Pop();
  p1 = _Stack->Pop();
  
  if( p2->Type() == porREFERENCE )
    p2 = _ResolveRef( (Reference_Portion*) p2 );
  
  if( p1->Type() == porREFERENCE )
  {
    if( mode == opSUBSCRIPT )
    {
      p = p1->Copy();
    }
    p1 = _ResolveRef( (Reference_Portion*) p1 );

    // SPECIAL CASE HANDLING - Subscript operator for Lists
    if( p1->Type() == porLIST && mode == opSUBSCRIPT )
    {
      if( p2->Type() == porINTEGER )
      {
	delete p1;
	p1 = p;
	( (Reference_Portion*) p1 )->SubValue() = 
	  Itoa( ( (numerical_Portion<gInteger>*) p2 )->Value() );
	delete p2;
	_Stack->Push( p1 );
	return true;
      }
      else
      {
	gerr << "GSM Error: a non-integer list index specified\n";
	gerr << "           for the Subscript() operator\n";
	delete p;
	delete p1;
	delete p2;
	p1 = new Error_Portion;
	_Stack->Push( p1 );
	return false;
      }
    }
    if( mode == opSUBSCRIPT )
    {
      delete p;
    }
  }

  if( p1->Type() == p2->Type() )
  {
    // SPECIAL CASE HANDLING - Integer division to produce gRationals
    if( mode == opDIVIDE && p1->Type() == porINTEGER &&
       ( (numerical_Portion<gInteger>*) p2 )->Value() != 0 )
    {
      p = new numerical_Portion<gRational>
	( ( (numerical_Portion<gInteger>*) p1 )->Value() );
      ( (numerical_Portion<gRational>*) p )->Value() /=
	( ( (numerical_Portion<gInteger>*) p2 )->Value() );
      delete p2;
      delete p1;
      p1 = p;
    }
    else
    {
      // Main operations dispatcher
      result = p1->Operation( p2, mode );
    }

    // SPECIAL CASE HANDLING - Boolean operators
    if(
       mode == opEQUAL_TO ||
       mode == opNOT_EQUAL_TO ||
       mode == opGREATER_THAN ||
       mode == opLESS_THAN ||
       mode == opGREATER_THAN_OR_EQUAL_TO ||
       mode == opLESS_THAN_OR_EQUAL_TO 
       )
    {
      delete p1;
      p1 = new bool_Portion( result );
      result = true;
    }
  }

  else // ( p1->Type() != p2->Type() )
  {
    gerr << "GSM Error: attempted operating on different types\n";
    gerr << "           Type of Operand 1: " << p1->Type() << "\n";
    gerr << "           Type of Operand 2: " << p2->Type() << "\n";
    delete p1;
    delete p2;
    p1 = new Error_Portion;
    result = false;
  }

  _Stack->Push( p1 );

  return result;
}




//-----------------------------------------------------------------------
//                        unary operations
//-----------------------------------------------------------------------

bool GSM::_UnaryOperation( OperationMode mode )
{
  Portion*  p1;
  bool      result = true;

  if( _Stack->Depth() >= 1 )
  {
    p1 = _Stack->Pop();
    
    if( p1->Type() == porREFERENCE )
      p1 = _ResolveRef( (Reference_Portion*) p1 );

    p1->Operation( 0, mode );
    _Stack->Push( p1 );
  }
  else
  {
    gerr << "GSM Error: not enough operands to perform unary operation\n";
    result = false;
  }

  return result;
}




//-----------------------------------------------------------------
//                      built-in operations
//-----------------------------------------------------------------

bool GSM::Add ( void )
{ return _BinaryOperation( opADD ); }

bool GSM::Subtract ( void )
{ return _BinaryOperation( opSUBTRACT ); }

bool GSM::Multiply ( void )
{ return _BinaryOperation( opMULTIPLY ); }

bool GSM::Divide ( void )
{ return _BinaryOperation( opDIVIDE ); }

bool GSM::Negate( void )
{ return _UnaryOperation( opNEGATE ); }


bool GSM::IntegerDivide ( void )
{ return _BinaryOperation( opINTEGER_DIVIDE ); }

bool GSM::Modulus ( void )
{ return _BinaryOperation( opMODULUS ); }


bool GSM::EqualTo ( void )
{ return _BinaryOperation( opEQUAL_TO ); }

bool GSM::NotEqualTo ( void )
{ return _BinaryOperation( opNOT_EQUAL_TO ); }

bool GSM::GreaterThan ( void )
{ return _BinaryOperation( opGREATER_THAN ); }

bool GSM::LessThan ( void )
{ return _BinaryOperation( opLESS_THAN ); }

bool GSM::GreaterThanOrEqualTo ( void )
{ return _BinaryOperation( opGREATER_THAN_OR_EQUAL_TO ); }

bool GSM::LessThanOrEqualTo ( void )
{ return _BinaryOperation( opLESS_THAN_OR_EQUAL_TO ); }


bool GSM::AND ( void )
{ return _BinaryOperation( opLOGICAL_AND ); }

bool GSM::OR ( void )
{ return _BinaryOperation( opLOGICAL_OR ); }

bool GSM::NOT ( void )
{ return _UnaryOperation( opLOGICAL_NOT ); }


bool GSM::Subscript ( void )
{ return _BinaryOperation( opSUBSCRIPT ); }



//-------------------------------------------------------------------
//               CallFunction() related functions
//-------------------------------------------------------------------

void GSM::AddFunction( FuncDescObj* func )
{
  _FuncTable->Define( func->FuncName(), func );
}


bool GSM::_FuncParamCheck( CallFuncObj* func, 
			  const PortionType stack_param_type )
{
  bool type_match;
  PortionType func_param_type = func->GetCurrParamType();
  gString funcname = func->FuncName();
  int index = func->GetCurrParamIndex();
  bool result = true;

  type_match = (stack_param_type & func_param_type) != 0;

  if( !type_match && func_param_type != porERROR )
  {
    gerr << "GSM Error: mismatched parameter type found while executing\n";
    gerr << "           CallFunction( \"" << funcname << "\", ... )\n";
    gerr << "           at Parameter #: " << index << "\n";
    gerr << "           Expected type: ";
    PrintPortionTypeSpec( gerr, func_param_type );
    gerr << "           Type found:    ";
    PrintPortionTypeSpec( gerr, stack_param_type );
    result = false;
  }
  return result;
}


#ifndef NDEBUG
void GSM::_BindCheck( void ) const
{
  if( _CallFuncStack->Depth() <= 0 )
  {
    gerr << "GSM Error: the CallFunction() subsystem was not initialized by\n";
    gerr << "           calling InitCallFunction() first\n";
  }
  assert( _CallFuncStack->Depth() > 0 );

  if( _Stack->Depth() <= 0 )
  {
    gerr << "GSM Error: no value found to assign to a function parameter\n";
  }
  assert( _Stack->Depth() > 0 );
}
#endif // NDEBUG


bool GSM::_BindCheck( const gString& param_name ) const
{
  CallFuncObj*  func;
  int           new_index;
  bool          result = true;

  _BindCheck();
  
  func = _CallFuncStack->Peek();
  new_index = func->FindParamName( param_name );
  
  if( new_index >= 0 )
  {
    func->SetCurrParamIndex( new_index );
  }
  else // ( new_index == PARAM_NOT_FOUND )
  {
    gerr << "FuncDescObj Error: parameter \"" << param_name;
    gerr << "\" is not defined for\n";
    gerr << "                   the function \"" << func->FuncName() << "\"\n";
    result = false;
  }
  return result;
}


bool GSM::InitCallFunction( const gString& funcname )
{
  CallFuncObj*  func;
  bool          result = true;

  if( _FuncTable->IsDefined( funcname ) )
  {
    func = new CallFuncObj( (*_FuncTable)( funcname ) );
    _CallFuncStack->Push( func );
  }
  else // ( !_FuncTable->IsDefined( funcname ) )
  {
    gerr << "GSM Error: undefined function name:\n";
    gerr << "           InitCallFunction( \"" << funcname << "\" )\n";
    result = false;
  }
  return result;
}


bool GSM::Bind( void )
{
  CallFuncObj*  func;
  PortionType          curr_param_type;
  Portion*             param;
  gString              funcname;
  bool                 result = true;
  gString ref;
  Reference_Portion* refp;

#ifndef NDEBUG
  _BindCheck();
#endif // NDEBUG

  func = _CallFuncStack->Peek();
  param = _Stack->Peek();

  if( func->GetCurrParamPassByRef() && ( param->Type() == porREFERENCE ) )
  {
    result = BindRef();
  }
  else
  {
    result = BindVal();
  }
  return result;
}


bool GSM::BindVal( void )
{
  CallFuncObj*  func;
  PortionType          curr_param_type;
  Portion*             param = 0;
  gString              funcname;
  int                  i;
  int                  type_match;
  bool                 result = true;
  gString ref;
  Reference_Portion* refp;

#ifndef NDEBUG
  _BindCheck();
#endif // NDEBUG

  func = _CallFuncStack->Pop();
  param = _Stack->Pop();
  
  if( param->Type() == porREFERENCE )
    param = _ResolveRef( (Reference_Portion *)param );

  result = _FuncParamCheck( func, param->Type() );
  if( result == true )
  {
    result = func->SetCurrParam( param ); 
  }
  else
  {
    delete param;
    func->SetCurrParamIndex( func->GetCurrParamIndex() + 1 );
  }

  if( !result )  // == false
  {
    func->SetErrorOccurred();
  }

  _CallFuncStack->Push( func );
  return result;
}


bool GSM::BindRef( void )
{
  CallFuncObj*  func;
  PortionType          curr_param_type;
  Portion*             param;
  Portion*             subparam;
  gString              funcname;
  int                  i;
  int                  type_match;
  bool                 result = true;
  gString ref;
  gString subref;

#ifndef NDEBUG
  _BindCheck();
#endif // NDEBUG

  func = _CallFuncStack->Pop();
  param = _Stack->Pop();
  
  if( func->GetCurrParamPassByRef() )
  {
    if( param->Type() == porREFERENCE )
    {
      ref = ( (Reference_Portion*) param )->Value();
      subref = ( (Reference_Portion*) param )->SubValue();
      func->SetCurrParamRef( (Reference_Portion*)( param->Copy() ) );
      param = _ResolveRefWithoutError( (Reference_Portion*) param );
      if( param != 0 )
      {
	if( param->Type() == porERROR )
	{
	  delete param;
	  result = false;
	}
      }
    }
    else // ( param->Type() != porREFERENCE )
    {
      gerr << "GSM Error: called BindRef() on a non-Reference type\n";
      _CallFuncStack->Push( func );
      _Stack->Push( param );
      result = BindVal();
      return false;
    }
  }
  else // ( !func->GetCurrParamPassByRef() )
  {
    gerr << "GSM Error: called BindRef() on a parameter that is specified\n";
    gerr << "           to be passed by value only\n";
    _CallFuncStack->Push( func );
    _Stack->Push( param );
    result = BindVal();
    return false;
  }

  
  if( result )  // == true
  {
    if( param != 0 )
    {
      result = _FuncParamCheck( func, param->Type() );
      if( result == true )
      {
	result = func->SetCurrParam( param );
      }
      else
      {
	delete param;
	func->SetCurrParamIndex( func->GetCurrParamIndex() + 1 );
      }
    }
    else
    {
      func->SetCurrParamIndex( func->GetCurrParamIndex() + 1 );
    }
  }

  if( !result )  // == false
  {
    func->SetErrorOccurred();
  }

  _CallFuncStack->Push( func );
  return result;
}




bool GSM::Bind( const gString& param_name )
{
  int result = false;

  if( _BindCheck( param_name ) )
  {
    result = Bind();
  }
  return result;
}


bool GSM::BindVal( const gString& param_name )
{
  int result = false;

  if( _BindCheck( param_name ) )
  {
    result = BindVal();
  }
  return result;
}


bool GSM::BindRef( const gString& param_name )
{
  int result = false;

  if( _BindCheck( param_name ) )
  {
    result = BindRef();
  }
  return result;
}


bool GSM::CallFunction( void )
{
  CallFuncObj*  func;
  Portion** param;
  int num_params;
  int index;
  gString ref;
  Reference_Portion* refp;
  Portion*             return_value;
  bool                 result = true;
  Portion* p;

#ifndef NDEBUG
  if( _CallFuncStack->Depth() <= 0 )
  {
    gerr << "GSM Error: the CallFunction() subsystem was not initialized by\n";
    gerr << "           calling InitCallFunction() first\n";
  }
  assert( _CallFuncStack->Depth() > 0 );
#endif // NDEBUG

  func = _CallFuncStack->Pop();

  num_params = func->NumParams();
  param = new Portion*[ num_params ];

  return_value = func->CallFunction( param );

  if( return_value == 0 )
  {
    gerr << "GSM Error: an error occurred while attempting to execute\n";
    gerr << "           CallFunction( \"" << func->FuncName() << "\", ... )\n";
    return_value = new Error_Portion;
    result = false;
  }

  _Stack->Push( return_value );


  for( index = 0; index < num_params; index++ )
  {
    if( func->ParamPassByReference( index ) )
    {
      func->SetCurrParamIndex( index );
      refp = func->GetCurrParamRef();
      if( refp != 0 && param[ index ] != 0 )
      {
	if( refp->SubValue() == "" )
	{
	  _RefTable->Define( refp->Value(), param[ index ] );
	}
	else
	{
	  if( _RefTable->IsDefined( refp->Value() ) )
	  {
	    p = ( *_RefTable )( refp->Value() );
	    switch( p->Type() )
	    {
	    case porNFG_DOUBLE:
	      ( (Nfg_Portion<double>*) p )->
		Assign( refp->SubValue(), param[ index ]->Copy() );
	      break;
	    case porNFG_RATIONAL:
	      ( (Nfg_Portion<gRational>*) p )->
		Assign( refp->SubValue(), param[ index ]->Copy() );
	      break;
	    case porLIST:
	      ( (List_Portion*) p )->
		SetSubscript( atoi( refp->SubValue() ), param[index]->Copy() );
	      break;

	    default:
	      gerr << "GSM Error: attempted to assign the subvariable of a\n";
	      gerr << "           type that does not support subvariables\n";
	      result = false;
	    }
	    delete param[ index ];
	  }
	  else
	  {
	    gerr << "GSM Error: attempted to assign the sub-variable of\n";
	    gerr << "           an undefined variable\n";
	    delete param[ index ];
	    result = false;
	  }
	}
	delete refp;
      }
      else
      {
	if( ( refp == 0 ) && ( param[ index ] != 0 ) )
	  delete param[ index ];
	else if( ( refp != 0 ) && ( param[ index ] == 0 ) )
	{
	  gerr << "some sort of fatal error; this should not occur\n";
	  gerr << "index: " << index << ", refp: " << refp << "\n";
	  assert(0);
	}
      }
    }
  }


  delete func;

  delete[] param;
  
  return result;
}


//----------------------------------------------------------------------------
//                       Execute function
//----------------------------------------------------------------------------

GSM_ReturnCode GSM::Execute( gList< Instruction* >& program )
{
  GSM_ReturnCode result = rcSUCCESS;
  bool instr_success;
  bool done = false;
  Portion *p;
  Instruction *instruction;
  int program_counter = 1;
  int program_length = program.Length();

  while( ( program_counter <= program_length ) && ( !done ) )
  {
    instruction = program[ program_counter ];

    switch( instruction->Type() )
    {
    case iQUIT:
      instr_success = true;
      result = rcQUIT;
      done = true;
      break;

    case iIF_GOTO:
      p = _Stack->Pop();
      if( p->Type() == porBOOL )
      {
	if( ( (bool_Portion*) p )->Value() )
	{
	  program_counter = ( (IfGoto*) instruction )->WhereTo();
	  assert( program_counter >= 1 && program_counter <= program_length );
	}
	else
	{
	  program_counter++;
	}
	delete p;
	instr_success = true;
      }
      else
      {
	gerr << "GSM Error: IfGoto called on a unsupported data type\n";
	_Stack->Push( p );
	program_counter++;
	instr_success = false;
      }
      break;

    case iGOTO:
      program_counter = ( (Goto*) instruction )->WhereTo();
      assert( program_counter >= 1 && program_counter <= program_length );
      instr_success = true;
      break;

    default:
      instr_success = instruction->Execute( *this );
      program_counter++;
    }

    if( !instr_success )
    {
      gerr << "GSM Error: instruction #" << program_counter;
      gerr << ": " << instruction << "\n";
      gerr << "           was not executed successfully\n";
      gerr << "           Program abnormally terminated.\n";
      result = rcFAIL;
      break;
    }
  }

  while( program.Length() > 0 )
  {
    instruction = program.Remove( 1 );
    delete instruction;
  }

  return result;
}


//----------------------------------------------------------------------------
//                   miscellaneous functions
//----------------------------------------------------------------------------


bool GSM::Pop( void )
{
  Portion* p;
  bool result = false;

  if( _Stack->Depth() > 0 )
  {
    p = _Stack->Pop();
    delete p;
    result = true;
  }
  else
  {
    gerr << "GSM Error: Pop() called on an empty stack\n";
  }
  return result;
}


void GSM::Output( void )
{
  Portion*  p;

  assert( _Stack->Depth() >= 0 );

  if( _Stack->Depth() == 0 )
  {
    gout << "Stack : NULL\n";
  }
  else
  {
    p = _Stack->Pop();
    if( p->Type() == porREFERENCE )
    {
      p = _ResolveRef( (Reference_Portion*) p );
    }
    p->Output( gout );
    gout << "\n";
    delete p;
  }
}


void GSM::Dump( void )
{
  int  i;

  assert( _Stack->Depth() >= 0 );

  if( _Stack->Depth() == 0 )
  {
    gout << "Stack : NULL\n";
  }
  else
  {
    for( i = _Stack->Depth() - 1; i >= 0; i-- )
    {
      gout << "Stack element " << i << " : ";
      Output();
    }
  }
  gout << "\n";
 
  assert( _Stack->Depth() == 0 );
}


void GSM::Flush( void )
{
  int       i;
  Portion*  p;

  for( i = _Stack->Depth() - 1; i >= 0; i-- )
  {
    p = _Stack->Pop();
    delete p;
  }

  assert( _Stack->Depth() == 0 );
}




//-----------------------------------------------------------------------
//                       Template instantiations
//-----------------------------------------------------------------------



#ifdef __GNUG__
#define TEMPLATE template
#elif defined __BORLANDC__
#define TEMPLATE
#pragma option -Jgd
#endif   // __GNUG__, __BORLANDC__



#include "glist.imp"

TEMPLATE class gList< Portion* >;
TEMPLATE class gNode< Portion* >;

TEMPLATE class gList< gString >;
TEMPLATE class gNode< gString >;

TEMPLATE class gList< FuncDescObj* >;
TEMPLATE class gNode< FuncDescObj* >;


#include "gstack.imp"

TEMPLATE class gStack< Portion* >;
TEMPLATE class gStack< CallFuncObj* >;


#include "ggrstack.imp"

TEMPLATE class gGrowableStack< Portion* >;
TEMPLATE class gGrowableStack< CallFuncObj* >;


gOutput& operator << ( class gOutput& s, class Portion* (*funcname)() )
{ return s << funcname; }


