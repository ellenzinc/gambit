//#
//# FILE: nfplayer.h -- Declaration of Normal Form Player data type
//#
//# $Id$
//#

#ifndef NFPLAYER_H
#define NFPLAYER_H

class Strategy;
class gRational;
template <class T> class Nfg;
template <class T> class NfgIter;
template <class T> class CIter;

class NFPlayer {

  friend class BaseNfg;
  friend class Support;
  friend class Nfg<double>;
  friend class Nfg<gRational>;
  friend class NfgIter<double>;
  friend class NfgIter<gRational>;
  friend class CIter<double>;
  friend class CIter<gRational>;

private:
  gString name;
  BaseNfg *N;
  
  gArray<Strategy *> strategies;
 

  NFPlayer(BaseNfg *n, int num);
  virtual ~NFPlayer();

public:
  BaseNfg *BelongsTo(void) const;
  
  const gString &GetName(void) const;
  void SetName (const gString &s);

  int NumStrats(void) const;

  const gArray<Strategy *> &StrategyList(void) const;


};
#endif    //# NFPLAYER_H


