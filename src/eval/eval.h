#pragma once
#ifdef EVAL_HCE
#include <eval/hce.h>
using EvalClass = raphael::hce::RaphaelHCE;


#elif defined(EVAL_NNUE)
#ifdef EVAL_MULTILAYER
    #include <eval/multilayer.h>
#elif defined(EVAL_SINGLELAYER)
    #include <eval/singlelayer.h>
#endif

using EvalClass = raphael::nnue::Nnue;

#endif