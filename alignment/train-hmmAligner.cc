#include "../core/LearningInfo.h"
#include "../wammar-utils/FstUtils.h"
#include "../wammar-utils/StringUtils.h"
#include "HmmAligner.h"
#include "IbmModel1.h"

using namespace fst;
using namespace std;
using namespace boost;

void ParseParameters(int argc, char **argv, string& bitextFilename, string &testBitextFilename, string &outputFilepathPrefix) {
  assert(argc == 4 || argc == 3);
  bitextFilename = argv[1];
  if(argc == 4) {
    testBitextFilename = argv[2];
    outputFilepathPrefix = argv[3];
  } else if(argc == 3) {
    outputFilepathPrefix = argv[2];
  } else {
    cerr << "invalid arguments" << endl;
  }
}

int main(int argc, char **argv) {
  // boost mpi initialization
  mpi::environment env(argc, argv);
  mpi::communicator world;

  // parse arguments
  cout << "parsing arguments" << endl;
  string bitextFilename, testBitextFilename, outputFilenamePrefix;
  ParseParameters(argc, argv, bitextFilename, testBitextFilename, outputFilenamePrefix);

  // specify stopping criteria
  LearningInfo learningInfo;
  learningInfo.maxIterationsCount = 1000;
  learningInfo.useMaxIterationsCount = true;
  learningInfo.minLikelihoodDiff = 100.0;
  learningInfo.useMinLikelihoodDiff = false;
  learningInfo.minLikelihoodRelativeDiff = 0.01;
  learningInfo.useMinLikelihoodRelativeDiff = true;
  learningInfo.debugLevel = DebugLevel::CORPUS;
  learningInfo.useEarlyStopping = false;
  learningInfo.mpiWorld = &world;
  learningInfo.persistParamsAfterNIteration = 1;
  learningInfo.persistFinalParams = false;

  // initialize the model
  HmmAligner model(bitextFilename, outputFilenamePrefix, learningInfo);

  // initialize lexical translation probabilities with ibm model1
  IbmModel1 model1(bitextFilename, outputFilenamePrefix, learningInfo, NULL_SRC_TOKEN_STRING, model.vocabEncoder);
  model1.Train();
  MultinomialParams::ConditionalMultinomialParam<int>& model1Params = model1.params;
  for(auto context = model1Params.params.begin(); context != model1Params.params.end(); context++){
    for(auto decision = context->second.begin(); decision != context->second.end(); decision++){
      // now update the hmmAligner translation params
      model.tFractionalCounts[context->first][decision->first] = decision->second;
    }
  }
  model.CreatePerSentGrammarFsts();
  cerr << "ibm model1 initialization finished" << endl;

  // train model parameters
  model.Train();

  // align the test set
  if(testBitextFilename.size() > 0) {
    string outputAlignmentsFilename = outputFilenamePrefix + ".test.align";
    model.AlignTestSet(testBitextFilename, outputAlignmentsFilename);
  } else {
    string outputAlignmentsFilename = outputFilenamePrefix + ".train.align";
    model.Align(outputAlignmentsFilename);
  }

  /*
  // sample a few translations
  vector<int> srcTokens;
  srcTokens.push_back(1);
  srcTokens.push_back(3);
  srcTokens.push_back(2);
  srcTokens.push_back(4);
  for(int i = 0; i < 500; i++) {
    vector<int> tgtTokens, alignments;
    double hmmLogProb;
    model.SampleATGivenS(srcTokens, 3, tgtTokens, alignments, hmmLogProb);
    cerr << endl << "translation: ";
    for(int j = 0; j < tgtTokens.size(); j++) {
      cerr << tgtTokens[j] << "(" << srcTokens[alignments[j]] << ") ";
    }
    cerr << hmmLogProb << "(" << FstUtils::nExp(hmmLogProb) << ")";
  }
  */
}
