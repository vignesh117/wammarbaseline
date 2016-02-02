#ifndef _IBM_MODEL_1_H_
#define _IBM_MODEL_1_H_

#include <iostream>
#include <fstream>
#include <math.h>
#include <algorithm>

#include "mpi.h"

#include <boost/unordered_map.hpp>
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi/nonblocking.hpp>
#include <boost/mpi/collectives.hpp>

#include "../wammar-utils/unordered_map_serialization.hpp"

#include "../core/LearningInfo.h"
#include "../wammar-utils/StringUtils.h"
#include "../wammar-utils/FstUtils.h"
#include "IAlignmentModel.h"
#include "../core/MultinomialParams.h"

using namespace fst;
using namespace std;

class IbmModel1 : public IAlignmentModel {

  // normalizes the parameters such that \sum_t p(t|s) = 1 \forall s
  void NormalizeParams();
  
  // creates an fst for each target sentence
  void CreateTgtFsts(vector< VectorFst< FstUtils::LogArc > >& targetFsts);

  void CreateGrammarFst();

  void CreatePerSentGrammarFsts(vector< VectorFst< FstUtils::LogArc > >& perSentGrammarFsts);
  
  // zero all parameters
  void ClearParams();
  
  void LearnParameters(vector< VectorFst< FstUtils::LogArc > >& tgtFsts);
  
 public:

  // bitextFilename is formatted as:
  // source sentence ||| target sentence
  IbmModel1(const string& bitextFilename, 
	    const string& outputFilenamePrefix, 
	    const LearningInfo& learningInfo,
	    const string &NULL_SRC_TOKEN,
	    const VocabEncoder &vocabEncoder);
  
  IbmModel1(const string& bitextFilename, 
	    const string& outputFilenamePrefix, 
	    const LearningInfo& learningInfe);
  
  void CoreConstructor(const string& bitextFilename, 
		       const string& outputFilenamePrefix, 
		       const LearningInfo& learningInfo,
		       const string &NULL_SRC_TOKEN);

  virtual void PrintParams();

  virtual void PersistParams(const string& outputFilename);
  
  // finds out what are the parameters needed by reading hte corpus, and assigning initial weights based on the number of co-occurences
  virtual void InitParams();

  virtual void Train();

  virtual void Align();
  void Align(const string &alignmentsFilename);

  virtual void AlignTestSet(const string &srcTestSetFilename, const string &tgtTestSetFilename, const string &outputAlignmentsFilename);

  void UpdateTheta(MultinomialParams::ConditionalMultinomialParam<int64_t> &mleGivenOneLabel, 
		   boost::unordered_map<int64_t, double> &mleMarginalsGivenOneLabel);
    
  void BroadcastTheta(unsigned rankId);

  void ReduceMleAndMarginals(MultinomialParams::ConditionalMultinomialParam<int64_t> &mle, 
			     boost::unordered_map<int64_t, double> &mleMarginals);
    
 private:
  string bitextFilename, outputPrefix;
  VectorFst<FstUtils::LogArc> grammarFst;
  LearningInfo learningInfo;
  vector< vector<int64_t> > srcSents, tgtSents;

 public:  
  // nlog prob(tgt word|src word)
  MultinomialParams::ConditionalMultinomialParam<int64_t> params;
  int NULL_SRC_TOKEN_ID;
  VocabEncoder vocabEncoder;

};

#endif
