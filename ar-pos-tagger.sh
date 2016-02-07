# This script will try to run a task *outside* any specified submitter
# Note: This script is for archival; it is not actually run by ducttape
export gold_file="/usr3/home/wammar/crf-auto-pos/arabic-final/PreprocessData/Baseline.baseline/autoencoder_labels"
export wordpair_feats_file="/usr3/home/wammar/crf-auto-pos/arabic-final/GenerateWordpairFeats/Baseline.baseline/wordpair_feats_file"
export data_file="/usr3/home/wammar/crf-auto-pos/arabic-final/PreprocessData/Baseline.baseline/autoencoder_text"
export tgt_brown_clusters="/usr3/home/wammar/crf-auto-pos/arabic-final/GenerateWordpairFeats/Baseline.baseline/paths"
export autoencoder_test_size="/usr3/home/wammar/crf-auto-pos/arabic-final/PreprocessData/Baseline.baseline/autoencoder_test_size"
export executable="/usr3/home/wammar/crf-auto-pos/arabic-final/BuildLatentCrfPosTagger/Baseline.baseline/executable"
export hmm_labels="/usr3/home/wammar/crf-auto-pos/arabic-final/AutoencoderPosInduction/DirichletAlpha.point_nine+Prefix.exp/hmm_labels"
export autoencoder_labels="/usr3/home/wammar/crf-auto-pos/arabic-final/AutoencoderPosInduction/DirichletAlpha.point_nine+Prefix.exp/autoencoder_labels"
export out_err="/usr3/home/wammar/crf-auto-pos/arabic-final/AutoencoderPosInduction/DirichletAlpha.point_nine+Prefix.exp/out_err"
export autoencoder_ll="/usr3/home/wammar/crf-auto-pos/arabic-final/AutoencoderPosInduction/DirichletAlpha.point_nine+Prefix.exp/autoencoder_ll"
export auto_test_labels="/usr3/home/wammar/crf-auto-pos/arabic-final/AutoencoderPosInduction/DirichletAlpha.point_nine+Prefix.exp/auto_test_labels"
export hmm_test_labels="/usr3/home/wammar/crf-auto-pos/arabic-final/AutoencoderPosInduction/DirichletAlpha.point_nine+Prefix.exp/hmm_test_labels"
export labeled_test_text="/usr1/home/wammar/pos-data/conll2007/autoencoder-format/arabic-padt-2007.tok.novowel"
export labels_count="12"
export procs="32"
export l2_strength="0.3"
export prefix="exp"
export reconstruct_brown_clusters="true"
export tag_dict_file=""
export optimize_lambdas_first="true"
export coord_itercount="50"
export supervised=""
export lbfgs_itercount="1"
export alignment_with_openfst_dir="/usr0/home/wammar/alignment-with-openfst/"
export test_with_crf_only=""
export wammar_utils_dir="/usr0/home/wammar/alignment-with-openfst/wammar-utils"
export min_relative_diff="0.0001"
export dirichlet_alpha="0.9"
export em_itercount="1"
export feature_set="full"

  variational="true"

  if [[ $tag_dict_file ]]; then
  python $alignment_with_openfst_dir/parts-of-speech/augment_tag_dict_with_case.py -i $tag_dict_file -o tag_dict_file.cased -t $data_file
  fi 

  test_size=$(cat $autoencoder_test_size)

  command="nice -10 mpirun -np $procs $executable 
  --output-prefix $prefix
  --train-data $data_file
  --feat LABEL_BIGRAM
  --min-relative-diff $min_relative_diff
  --max-iter-count $coord_itercount
  --cache-feats false
  --check-gradient false
  --optimizer lbfgs --minibatch-size 100000
  #--optimizer sgd
  --wordpair-feats $wordpair_feats_file 
  --labels-count $labels_count
  --gold-labels-filename $gold_file"

  # USE THIS OPTION TO RESUME WITH SOME PARAMETER DUMP IN CASE OF FAILURE
  #   --init-theta xxx/other.38.theta
  #   --init-lambda xxx/other.38.lambda

  # specify feature templates in each feature set.
  if [[ $feature_set == "basic" ]]; then
    command="$command --feat EMISSION"
  fi
  if [[ $feature_set == "hk" ]]; then
    command="$command --feat EMISSION --feat PRECOMPUTED"
  fi
  if [[ $feature_set == "full" ]]; then
    command="$command --feat PRECOMPUTED --feat PRECOMPUTED_XIM1  --feat PRECOMPUTED_XIP1"
  fi

  if [[ $supervised ]]; then
    command="$command --supervised true"
  fi
 
  if [[ $tgt_brown_clusters && $reconstruct_brown_clusters ]]; then
    command="$command --tgt-word-classes-filename $tgt_brown_clusters"
  fi

  if [[ $l2_strength ]]; then
    command="$command --l2-strength $l2_strength"
  fi

  if [[ $dirichlet_alpha ]]; then
    command="$command --dirichlet-alpha $dirichlet_alpha"
  fi

  if [[ $variational ]]; then
    command="$command --variational-inference $variational"
  fi

  if [[ $test_with_crf_only ]]; then
    command="$command --test-with-crf-only $test_with_crf_only"
  fi

  if [[ $em_itercount ]]; then
    command="$command --max-em-iter-count $em_itercount"
  fi

  if [[ $lbfgs_itercount ]]; then
    command="$command --max-lbfgs-iter-count $lbfgs_itercount"
  fi 

  if [[ $optimize_lambdas_first ]]; then
    command="$command --optimize-lambdas-first $optimize_lambdas_first"
  fi

  if [[ $tag_dict_file ]]; then
    command="$command --tag-dict-filename tag_dict_file.cased"
  fi

  echo "executing $command..."
  $command 2> $out_err

  actual_test_size=$(wc -l $labeled_test_text |awk -F" " '{print $1}')
  echo "actual test size is $actual_test_size"  
  echo "autoencoder test size is $test_size"  

  head -n $test_size $data_file | tail -n $actual_test_size > data_file_test

  if [[ $supervised ]]; then
    tail -n $actual_test_size $prefix.supervised.labels > $auto_test_labels    
  else
    tail -n $actual_test_size $prefix.final.labels > $auto_test_labels    
    tail -n $actual_test_size $prefix.hmm.labels > $hmm_test_labels    
  fi
 
  python $wammar_utils_dir/combine-token-label-in-one-file.py data_file_test $auto_test_labels $autoencoder_labels
  python $wammar_utils_dir/combine-token-label-in-one-file.py data_file_test $hmm_test_labels $hmm_labels
  
  touch $autoencoder_ll

