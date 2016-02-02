#include "IbmModel1.h"

#include <iostream>
#include <fstream>
#include <math.h>

using namespace std;
using namespace fst;

IbmModel1::IbmModel1(const string& bitextFilename, 
                     const string& outputFilenamePrefix, 
                     const LearningInfo& learningInfo,
                     const string &NULL_SRC_TOKEN,
                     const VocabEncoder &vocabEncoder) : learningInfo(learningInfo), vocabEncoder(vocabEncoder) {
  CoreConstructor(bitextFilename, outputFilenamePrefix, learningInfo, NULL_SRC_TOKEN);
}

IbmModel1::IbmModel1(const string& bitextFilename, 
                     const string& outputFilenamePrefix, 
                     const LearningInfo& learningInfo) : learningInfo(learningInfo), vocabEncoder(bitextFilename, learningInfo) {
  
  CoreConstructor(bitextFilename, outputFilenamePrefix, learningInfo, "__null__");
}

// initialize model 1 scores
void IbmModel1::CoreConstructor(const string& bitextFilename, 
                                const string& outputFilenamePrefix, 
                                const LearningInfo& learningInfo,
                                const string &NULL_SRC_TOKEN) {
  // set member variables
  this->bitextFilename = bitextFilename;
  this->outputPrefix = outputFilenamePrefix;
  
  // read encoded training data
  NULL_SRC_TOKEN_ID = vocabEncoder.Encode(NULL_SRC_TOKEN);
  vocabEncoder.ReadParallelCorpus(bitextFilename, srcSents, tgtSents, NULL_SRC_TOKEN, learningInfo.reverse);
  assert(srcSents.size() > 0 && srcSents.size() == tgtSents.size());  
  assert(vocabEncoder.ConstEncode(NULL_SRC_TOKEN) != vocabEncoder.UnkInt());
  
  // initialize the model parameters
  if (learningInfo.mpiWorld->rank() == 0) {
    cerr << "init model1 params" << endl;
  }
  stringstream initialModelFilename;
  initialModelFilename << outputPrefix << ".param.init";
  InitParams();
  PersistParams(initialModelFilename.str());
  
  // create the initial grammar FST
  CreateGrammarFst();

}

void IbmModel1::Train() {

  // create tgt fsts
  vector< VectorFst <FstUtils::LogArc> > tgtFsts;
  CreateTgtFsts(tgtFsts);

  // training iterations
  if (learningInfo.mpiWorld->rank() == 0) {
    cerr << "train!" << endl;
  }
  LearnParameters(tgtFsts);

  // persist parameters
  if (learningInfo.mpiWorld->rank() == 0) {
    cerr << "persist" << endl;
    PersistParams(outputPrefix + ".param.final");
  }
}

void IbmModel1::CreateTgtFsts(vector< VectorFst< FstUtils::LogArc > >& targetFsts) {

  for(unsigned i = 0; i < tgtSents.size(); i++) {
    VectorFst< FstUtils::LogArc > tgtFst;
    if (i % learningInfo.mpiWorld->size() != learningInfo.mpiWorld->rank()) {
      targetFsts.push_back(tgtFst);
      continue;
    }

    // read the list of integers representing target tokens
    vector< int64_t > &intTokens  =tgtSents[i];
    
    // create the fst
    for(unsigned stateId = 0; stateId < intTokens.size()+1; stateId++) {
      int temp = tgtFst.AddState();
      assert(temp == (int)stateId);
      if(stateId == 0) continue;
      tgtFst.AddArc(stateId-1, FstUtils::LogArc(intTokens[stateId-1], intTokens[stateId-1], 0, stateId));
    }
    tgtFst.SetStart(0);
    tgtFst.SetFinal(intTokens.size(), 0);
    ArcSort(&tgtFst, ILabelCompare<FstUtils::LogArc>());
    targetFsts.push_back(tgtFst);
    
    // for debugging
    // PrintFstSummary(tgtFst);
  }
}

// normalizes the parameters such that \sum_t p(t|s) = 1 \forall s
void IbmModel1::NormalizeParams() {
  MultinomialParams::NormalizeParams(params);
}

void IbmModel1::PrintParams() {
  params.PrintParams(vocabEncoder, true, true);
}

void IbmModel1::PersistParams(const string& outputFilename) {
  if (learningInfo.mpiWorld->rank() == 0) { return; }
  MultinomialParams::PersistParams(outputFilename, params, vocabEncoder, true, true);
}

// finds out what are the parameters needed by reading hte corpus, and assigning initial weights based on the number of co-occurences
void IbmModel1::InitParams() {
  for(unsigned sentId = 0; sentId < srcSents.size(); sentId++) {
    // read the list of integers representing target tokens
    vector< int64_t > &tgtTokens = tgtSents[sentId], &srcTokens = srcSents[sentId];
    
    // for each srcToken
    for(size_t i=0; i<srcTokens.size(); i++) {
      int64_t srcToken = srcTokens[i];
      // get the corresponding map of tgtTokens (and the corresponding probabilities)
      boost::unordered_map<int64_t, double> &translations = params.params[srcToken];
      
      // for each tgtToken
      for (size_t j=0; j<tgtTokens.size(); j++) {
        int64_t tgtToken = tgtTokens[j];
	// if this the first time the pair(tgtToken, srcToken) is experienced, give it a value of 1 (i.e. prob = exp(-1) ~= 1/3)
	if( translations.count(tgtToken) == 0) {
	  translations[tgtToken] = FstUtils::nLog(1/3.0);
	} else {
	  // otherwise, add nLog(1/3) to the original value, effectively counting the number of times 
	  // this srcToken-tgtToken pair appears in the corpus
	  translations[tgtToken] = Plus( FstUtils::LogWeight(translations[tgtToken]), FstUtils::LogWeight(FstUtils::nLog(1/3.0)) ).Value();
	}
      }
    }
  }
    
  NormalizeParams();
}

void IbmModel1::CreateGrammarFst() {
  // clear grammar
  if (grammarFst.NumStates() > 0) {
    grammarFst.DeleteArcs(grammarFst.Start());
    grammarFst.DeleteStates();
  }
  
  // create the only state in this fst, and make it initial and final
  FstUtils::LogArc::StateId dummy = grammarFst.AddState();
  assert(dummy == 0);
  grammarFst.SetStart(0);
  grammarFst.SetFinal(0, 0);
  int fromState = 0, toState = 0;
  for(auto srcIter = params.params.begin(); srcIter != params.params.end(); srcIter++) {
    for(auto tgtIter = srcIter->second.begin(); tgtIter != srcIter->second.end(); tgtIter++) {
      int64_t tgtToken = tgtIter->first;
      int64_t srcToken = srcIter->first;
      double paramValue = tgtIter->second;
      grammarFst.AddArc(fromState, FstUtils::LogArc(tgtToken, srcToken, paramValue, toState));
    }
  }
  ArcSort(&grammarFst, ILabelCompare<FstUtils::LogArc>());
  //  PrintFstSummary(grammarFst);
}

void IbmModel1::CreatePerSentGrammarFsts(vector< VectorFst< FstUtils::LogArc > >& perSentGrammarFsts) {

  for(unsigned sentId = 0; sentId < srcSents.size(); sentId++) {
    VectorFst< FstUtils::LogArc > grammarFst;
    if (sentId % learningInfo.mpiWorld->size() != learningInfo.mpiWorld->rank()) {
      perSentGrammarFsts.push_back(grammarFst);
      continue;
    }

    vector<int64_t> &srcTokens = srcSents[sentId];
    vector<int64_t> &tgtTokensVector = tgtSents[sentId];
    set<int64_t> tgtTokens(tgtTokensVector.begin(), tgtTokensVector.end());

    // allow null alignments
    assert(srcTokens[0] == NULL_SRC_TOKEN_ID);
    
    // create the fst
    int stateId = grammarFst.AddState();
    assert(stateId == 0);
    for(auto srcTokenIter = srcTokens.begin(); srcTokenIter != srcTokens.end(); srcTokenIter++) {
      for(auto tgtTokenIter = tgtTokens.begin(); tgtTokenIter != tgtTokens.end(); tgtTokenIter++) {
        if(learningInfo.preventSelfAlignments && *tgtTokenIter == *srcTokenIter) {
          // prevent this self alignment.
        } else {
          grammarFst.AddArc(stateId, FstUtils::LogArc(*tgtTokenIter, *srcTokenIter, params[*srcTokenIter][*tgtTokenIter], stateId));
        }
      }
    }
    grammarFst.SetStart(stateId);
    grammarFst.SetFinal(stateId, 0);
    ArcSort(&grammarFst, ILabelCompare<FstUtils::LogArc>());
    perSentGrammarFsts.push_back(grammarFst);
    
  }
}

// zero all parameters
void IbmModel1::ClearParams() {
  for (auto srcIter = params.params.begin(); srcIter != params.params.end(); srcIter++) {
    for (auto tgtIter = srcIter->second.begin(); tgtIter != srcIter->second.end(); tgtIter++) {
      tgtIter->second = 0.0;
    }
  }
}

void IbmModel1::UpdateTheta(
    MultinomialParams::ConditionalMultinomialParam<int64_t> &mleGivenOneLabel, 
    boost::unordered_map<int64_t, double> &mleMarginalsGivenOneLabel) {

  // Only override the theta distributions for contexts observed in mleGivenOneLabel so far. 
  for (auto contextIter = mleGivenOneLabel.params.begin();
       contextIter != mleGivenOneLabel.params.end();
       ++contextIter) {
    // Update all decisions conditioned on a particular context.
    for (auto decisionIter = params[contextIter->first].begin();
         decisionIter != params[contextIter->first].end();
         ++decisionIter) {
      // normalize mle counts to get the probability of a decision.
      double numerator = 0, denominator = 1;
      if (learningInfo.variationalInferenceOfMultinomials) {
        numerator = 
          exp( boost::math::digamma( contextIter->second[decisionIter->first] +
                                     contextIter->second.size() * learningInfo.multinomialSymmetricDirichletAlpha) );
        denominator = 
          exp( boost::math::digamma( mleMarginalsGivenOneLabel[contextIter->first] + 
                                     learningInfo.multinomialSymmetricDirichletAlpha ));
      } else if (learningInfo.multinomialSymmetricDirichletAlpha != 1.0 ) {
        numerator = 
          contextIter->second[decisionIter->first] +
          contextIter->second.size() * (learningInfo.multinomialSymmetricDirichletAlpha - 1.0);
        denominator = 
	  mleMarginalsGivenOneLabel[contextIter->first] + 
          learningInfo.multinomialSymmetricDirichletAlpha - 1.0;
      } else {
        numerator = contextIter->second[decisionIter->first];
        denominator = mleMarginalsGivenOneLabel[contextIter->first];
      }
      assert(denominator != 0.0);
      
      // numerical errors may cause probability to be < 0.0
      double probability = max(0.0, numerator / denominator);
      params[contextIter->first][decisionIter->first] = 
        MultinomialParams::nLog(probability);
    }
  }
}

void IbmModel1::BroadcastTheta(unsigned rankId) {
  boost::mpi::broadcast< boost::unordered_map< int64_t, MultinomialParams::MultinomialParam > >(*learningInfo.mpiWorld, params.params, rankId);
}

void IbmModel1::ReduceMleAndMarginals(
    MultinomialParams::ConditionalMultinomialParam<int64_t> &mle, 
    boost::unordered_map<int64_t, double> &mleMarginals) {

  boost::mpi::reduce< boost::unordered_map< int64_t, MultinomialParams::MultinomialParam > >(*learningInfo.mpiWorld, 
                                                                                      mle.params, 
                                                                                      mle.params, 
                                                                                      MultinomialParams::AccumulateConditionalMultinomials< int64_t >, 0);
  boost::mpi::reduce< boost::unordered_map< int64_t, double > >(*learningInfo.mpiWorld, 
                                                         mleMarginals,
                                                         mleMarginals, 
                                                         MultinomialParams::AccumulateMultinomials<int64_t>, 0);
}

void IbmModel1::LearnParameters(vector< VectorFst< FstUtils::LogArc > >& tgtFsts) {
  clock_t compositionClocks = 0, forwardBackwardClocks = 0, updatingFractionalCountsClocks = 0, grammarConstructionClocks = 0, normalizationClocks = 0;
  clock_t t00 = clock();
  do {
    clock_t t05 = clock();
    vector< VectorFst< FstUtils::LogArc > > perSentGrammarFsts;
    CreatePerSentGrammarFsts(perSentGrammarFsts);
    grammarConstructionClocks += clock() - t05;

    double logLikelihood = 0, validationLogLikelihood = 0;
    //    cout << "iteration's loglikelihood = " << logLikelihood << endl;
    
    // params will be used to accumulate fractional counts of parameter usages.
    // This would be problematic if some processors had not finished creating their per-sent grammars.
    bool sync = true;
    boost::mpi::all_reduce<bool>(*learningInfo.mpiWorld, sync, sync, std::logical_and<bool>());
    ClearParams();
    // also keep track of the partial counts per context
    boost::unordered_map<int64_t, double> mleMarginals;

    // iterate over sentences
    int sentsCounter = 0;
    for( vector< VectorFst< FstUtils::LogArc > >::const_iterator tgtIter = tgtFsts.begin(), grammarIter = perSentGrammarFsts.begin(); 
         tgtIter != tgtFsts.end() && grammarIter != perSentGrammarFsts.end(); 
         tgtIter++, grammarIter++, sentsCounter++) {

      if (sentsCounter % learningInfo.mpiWorld->size() != learningInfo.mpiWorld->rank()) {
	continue;
      }
      
      // build the alignment fst
      clock_t t20 = clock();
      VectorFst< FstUtils::LogArc > tgtFst = *tgtIter, perSentGrammarFst = *grammarIter, alignmentFst;
      Compose(tgtFst, perSentGrammarFst, &alignmentFst);
      compositionClocks += clock() - t20;
      //FstUtils::PrintFstSummary(alignmentFst);
      
      // run forward/backward for this sentence
      clock_t t30 = clock();
      vector<FstUtils::LogWeight> alphas, betas;
      ShortestDistance(alignmentFst, &alphas, false);
      ShortestDistance(alignmentFst, &betas, true);
      double fSentLogLikelihood = betas[alignmentFst.Start()].Value();
      
      forwardBackwardClocks += clock() - t30;
      
      // compute and accumulate fractional counts for model parameters
      clock_t t40 = clock();
      bool excludeFractionalCountsInThisSent = 
        learningInfo.useEarlyStopping && 
        sentsCounter % learningInfo.trainToDevDataSize == 0;
      for (int stateId = 0; !excludeFractionalCountsInThisSent && stateId < alignmentFst.NumStates() ;stateId++) {
        for (ArcIterator<VectorFst< FstUtils::LogArc > > arcIter(alignmentFst, stateId);
             !arcIter.Done();
             arcIter.Next()) {
          int64_t srcToken = arcIter.Value().olabel, tgtToken = arcIter.Value().ilabel;
          int fromState = stateId, toState = arcIter.Value().nextstate;
          
          // probability of using this parameter given this sentence pair and the previous model
          FstUtils::LogWeight currentParamLogProb = arcIter.Value().weight;
          FstUtils::LogWeight unnormalizedPosteriorLogProb = Times(Times(alphas[fromState], currentParamLogProb), betas[toState]);
          double fNormalizedPosteriorLogProb = unnormalizedPosteriorLogProb.Value() - fSentLogLikelihood;
          
          // append the fractional count for this parameter
          params[srcToken][tgtToken] += exp(fNormalizedPosteriorLogProb);
	  if (mleMarginals.count(srcToken) == 0) {
	    mleMarginals[srcToken] = 0.0;
	  }
	  mleMarginals[srcToken] += exp(fNormalizedPosteriorLogProb);
	}
      }
      
      // update the iteration log likelihood with this sentence's likelihod
      if(excludeFractionalCountsInThisSent) {
	validationLogLikelihood += fSentLogLikelihood;
      } else {
	logLikelihood += fSentLogLikelihood;
      }
    }

    // accumulate mle counts from slaves
    ReduceMleAndMarginals(params, mleMarginals);
    boost::mpi::all_reduce<double>(*learningInfo.mpiWorld, logLikelihood, logLikelihood, std::plus<double>());

    // normalize mle and update nLogTheta on master
    if(learningInfo.mpiWorld->rank() == 0) {
      UpdateTheta(params, mleMarginals);
    }

    // update nLogTheta on slaves
    BroadcastTheta(0);
    
    // create the new grammar
    CreateGrammarFst();

    // logging
    if (learningInfo.mpiWorld->rank() == 0) {
      cerr << "Ibm Model 1 iteration # " << learningInfo.iterationsCount << " - total loglikelihood = " << logLikelihood << endl;
    }

    // update learningInfo
    learningInfo.logLikelihood.push_back(logLikelihood);
    learningInfo.validationLogLikelihood.push_back(validationLogLikelihood);
    learningInfo.iterationsCount++;
    // check for convergence
  } while(!learningInfo.IsModelConverged());
}

// given the current model, align the corpus
void IbmModel1::Align() {
  Align(outputPrefix + ".train.align");
}

void IbmModel1::Align(const string &alignmentsFilename) {
  ofstream outputAlignments;
  outputAlignments.open(alignmentsFilename.c_str(), ios::out);

  vector< VectorFst< FstUtils::LogArc > > perSentGrammarFsts;
  CreatePerSentGrammarFsts(perSentGrammarFsts);
  vector< VectorFst <FstUtils::LogArc> > tgtFsts;
  CreateTgtFsts(tgtFsts);

  assert(tgtFsts.size() == srcSents.size());
  assert(perSentGrammarFsts.size() == srcSents.size());
  assert(tgtSents.size() == srcSents.size());

  for(unsigned sentId = 0; sentId < srcSents.size(); sentId++) {
    vector<int64_t> &srcSent = srcSents[sentId];
    VectorFst< FstUtils::LogArc > &perSentGrammarFst = perSentGrammarFsts[sentId], &tgtFst = tgtFsts[sentId], alignmentFst;
    
    // given a src token id, what are the possible src position (in this sentence)
    boost::unordered_map<int64_t, set<int> > srcTokenToSrcPos;
    for(unsigned srcPos = 0; srcPos < srcSent.size(); srcPos++) {
      srcTokenToSrcPos[ srcSent[srcPos] ].insert(srcPos);
    }
    
    // build alignment fst and compute potentials
    Compose(tgtFst, perSentGrammarFst, &alignmentFst);
    vector<FstUtils::LogWeight> alphas, betas;
    ShortestDistance(alignmentFst, &alphas, false);
    ShortestDistance(alignmentFst, &betas, true);
    
    // tropical has the path property. we need this property to compute the shortest path
    VectorFst< FstUtils::StdArc > alignmentFstProbsWithPathProperty, bestAlignment, corrected;
    ArcMap(alignmentFst, &alignmentFstProbsWithPathProperty, FstUtils::LogToTropicalMapper());
    ShortestPath(alignmentFstProbsWithPathProperty, &bestAlignment);
   
    // fix labels
    // - the input labels are tgt positions
    // - the output labels are the corresponding src positions according to the alignment
    // traverse the transducer beginning with the start state
    stringstream alignmentsLine;
    int startState = bestAlignment.Start();
    int currentState = startState;
    int tgtPos = 0;
    while(bestAlignment.Final(currentState) == FstUtils::LogWeight::Zero()) {
      // get hold of the arc
      ArcIterator< VectorFst< FstUtils::StdArc > > aiter(bestAlignment, currentState);
      // identify the next state
      int nextState = aiter.Value().nextstate;
      // skip epsilon arcs
      if(aiter.Value().ilabel == FstUtils::EPSILON && aiter.Value().olabel == FstUtils::EPSILON) {
	currentState = nextState;
	continue;
      }
      // update tgt pos
      tgtPos++;
      // find src pos
      int srcPos = 0;
      assert(srcTokenToSrcPos[aiter.Value().olabel].size() > 0);
      if(srcTokenToSrcPos[aiter.Value().olabel].size() == 1) {
	srcPos = *(srcTokenToSrcPos[aiter.Value().olabel].begin());
      } else {
	float distortion = 100;
	for(set<int>::iterator srcPosIter = srcTokenToSrcPos[aiter.Value().olabel].begin(); srcPosIter != srcTokenToSrcPos[aiter.Value().olabel].end(); srcPosIter++) {
	  if(*srcPosIter - tgtPos < distortion) {
	    srcPos = *srcPosIter;
	    distortion = abs(*srcPosIter - tgtPos);
	  }
	}
      }
      // print the alignment giza-style
      // giza++ does not write null alignments
      if(srcPos != 0) {
	// giza++ uses zero-based src and tgt positions, and writes the src position first
	alignmentsLine << (srcPos - 1) << "-" << (tgtPos - 1) << " ";
      }
      // this state shouldn't have other arcs!
      aiter.Next();
      assert(aiter.Done());
      // move forward to the next state
      currentState = nextState;
    }
    alignmentsLine << endl;

    // write the best alignment to file
    outputAlignments << alignmentsLine.str();
  }
  outputAlignments.close();
}


// TODO: not implemented 
// given the current model, align a test set
void IbmModel1::AlignTestSet(const string &srcTestSetFilename, const string &tgtTestSetFilename, const string &outputAlignmentsFilename) {
  assert(false);
}
