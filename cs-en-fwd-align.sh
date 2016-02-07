# This script will try to run a task *outside* any specified submitter
# Note: This script is for archival; it is not actually run by ducttape
export wordpair_feats_file="wordpair_feats_file"
export executable="/usr0/home/wammar/alignment-with-openfst/train-latentCrfAligner"
export alignment="/usr1/home/wammar/mt-systems/cz-en-exp202/AutoencoderAlignS2T/DirichletAlpha.one_point_five+EmItercount.two+L2Strength.point_o_one+OptimizeLambdasFirst.yes+PrecomputedFeatures.dyer11+UseOtherAligners.yes/alignment"
export out_err="/usr1/home/wammar/mt-systems/cz-en-exp202/AutoencoderAlignS2T/DirichletAlpha.one_point_five+EmItercount.two+L2Strength.point_o_one+OptimizeLambdasFirst.yes+PrecomputedFeatures.dyer11+UseOtherAligners.yes/out_err"
export init_lambda=""
export sym_fast_alignments="/usr1/home/wammar/parallel/100k+aer.cz-en.fast.sym-out"
export bwd_fast_alignments="/usr1/home/wammar/parallel/100k+aer.cz-en.fast.bwd-out"
export procs="16"
export fwd_fast_alignments="/usr1/home/wammar/parallel/100k+aer.cz-en.fast.fwd-out"
export l2_strength="0.01"
export optimize_lambdas_first="true"
export l1_strength=""
export use_other_aligners="yes"
export test_sents_count="1"
export data_file="/usr1/home/wammar/parallel/100k+aer.cz-en"
export lbfgs_itercount="1"
export alignment_with_openfst_dir="/home/wammar/alignment-with-openfst/"
export test_with_crf_only="false"
export sym_giza_alignments="/usr1/home/wammar/parallel/100k+aer.cz-en.giza.sym-out"
export init_theta="/usr1/home/wammar/mt-systems/cz-en-exp202/AutoencoderAlignS2T/DirichletAlpha.one_point_five+EmItercount.two+L2Strength.point_o_one+OptimizeLambdasFirst.yes+PrecomputedFeatures.dyer11+UseOtherAligners.yes-old/prefix.1.theta"
export fwd_giza_alignments="/usr1/home/wammar/parallel/100k+aer.cz-en.giza.bwd-out"
export use_src_bigrams=""
export tgt_brown_clusters="/usr1/home/wammar/parallel/mono-english-for-wmt11-czen.brown80"
export dirichlet_alpha="1.5"
export bwd_giza_alignments="/usr1/home/wammar/parallel/100k+aer.cz-en.giza.fwd-out"
export em_itercount="1"


  model1_itercount="5"
  variational="true"

  # the latent-CRF word alignment mode
  command="mpirun -np $procs $executable
  --train-data $data_file 
  --output-prefix s2t 
  --max-iter-count 0
  --feat PRECOMPUTED --feat DIAGONAL_DEVIATION --feat LOG_ALIGNMENT_JUMP
  --max-model1-iter-count $model1_itercount"
  #--test-size $test_sents_count
  # --feat SRC0_TGT0
  #--tgt-word-classes-filename $tgt_brown_clusters
  # hiding --feat SRC0_TGT0 
  # --feat SYNC_START --feat SYNC_END  
  #--optimizer adagrad --minibatch-size 1000

  if [ $wordpair_feats_file ]; then
    command="$command --wordpair-feats $wordpair_feats_file"
  fi

  
  if [ $init_theta ]; then
    command="$command --init-theta $init_theta"  
  fi

  if [ $init_lambda ]; then
    command="$command --init-lambda $init_lambda"
  fi

  if [ $l2_strength ]; then
    command="$command --l2-strength $l2_strength"
  fi

  if [ $l1_strength ]; then
    command="$command --l1-strength $l1_strength"
  fi

  if [ $dirichlet_alpha ]; then
    command="$command --dirichlet-alpha $dirichlet_alpha"
  fi

  if [ $variational ]; then
    command="$command --variational-inference $variational"
  fi

  if [ $test_with_crf_only ]; then
    command="$command --test-with-crf-only $test_with_crf_only"
  fi

  if [ $em_itercount ]; then
    command="$command --max-em-iter-count $em_itercount"
  fi

  if [ $lbfgs_itercount ]; then
    command="$command --max-lbfgs-iter-count $lbfgs_itercount"
  fi 

  if [ $optimize_lambdas_first ]; then
    command="$command --optimize-lambdas-first $optimize_lambdas_first"
  fi

  if [ $fwd_giza_alignments ]; then
    command="$command --other-aligners-output-filenames $fwd_giza_alignments"
  fi

  if [ $bwd_giza_alignments ]; then
    command="$command --other-aligners-output-filenames $bwd_giza_alignments"
  fi

  if [ $sym_giza_alignments ]; then
    command="$command --other-aligners-output-filenames $sym_giza_alignments"
  fi

  if [ $fwd_fast_alignments ]; then
    command="$command --other-aligners-output-filenames $fwd_fast_alignments"
  fi

  if [ $bwd_fast_alignments ]; then
    command="$command --other-aligners-output-filenames $bwd_fast_alignments"
  fi

  if [ $sym_fast_alignments ]; then
    command="$command --other-aligners-output-filenames $sym_fast_alignments"
  fi

  if [ $use_other_aligners ]; then
    command="$command --feat OTHER_ALIGNERS"
  fi

  if [ $use_src_bigrams ]; then
    command="$command --feat SRC_BIGRAM"
  fi

  echo "executing $command..."
  $command 2> $out_err


