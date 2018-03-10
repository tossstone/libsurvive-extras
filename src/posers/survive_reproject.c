#include "survive_reproject.h"
#include <../redist/linmath.h>
#include <string.h>
#include <math.h>

static void survive_calibration_options_config_normalize(survive_calibration_options_config* option) {
  if(!option->enable[0])
    option->invert[0] = false;
  if(!option->enable[1])
    option->invert[1] = false;
  if(!option->enable[0] && !option->enable[1])
    option->swap = false; 
}

static void survive_calibration_options_config_apply(const survive_calibration_options_config* option,
							 const FLT* input, FLT* output) { 
  FLT tmp[2]; // In case they try to do in place
  for(int i = 0;i < 2;i++) {
    tmp[i] = option->enable[i] * (option->invert[i] ? -1 : 1) * input[!option->swap];
  }
  for(int i = 0;i < 2;i++) {
    output[i] = tmp[i]; 
  }
}

survive_calibration_config survive_calibration_config_create_from_idx(size_t v) {
  survive_calibration_config config;
  memset(&config, 0, sizeof(config));
    
  bool* _this = (bool*)&config;
  size_t ov = v;
  for(size_t i = 0;i < sizeof(config);i++) {
    _this[i] = (v & 1);
    v = v >> 1;
  }

  survive_calibration_options_config_normalize(&config.phase);
  survive_calibration_options_config_normalize(&config.tilt);
  survive_calibration_options_config_normalize(&config.curve);
  survive_calibration_options_config_normalize(&config.gibMag);

  config.gibPhase.enable[0] = config.gibMag.enable[0];
  config.gibPhase.enable[1] = config.gibMag.enable[1];	

  survive_calibration_options_config_normalize(&config.gibPhase);

  if(!config.gibPhase.enable[0] && !config.gibPhase.enable[1])
    config.gibUseSin = false;

  return config;
}

size_t survive_calibration_config_index(const survive_calibration_config* config) {
  bool* _this = (bool*)config;
  size_t v = 0;
  for(size_t i = 0;i < sizeof(config);i++) {
    v = (v | _this[sizeof(config) - i - 1]);
    v = v << 1; 
  }
  v = v >> 1;
  return v;
}

static FLT gibf(bool useSin, FLT v) {
  if(useSin)
    return sin(v);
  return cos(v);
}

void survive_reproject_from_pose_with_config(const SurviveContext* ctx,
					     const survive_calibration_config* config,
					     int lighthouse,
					     const SurvivePose* pose,
					     const FLT* pt,
					     FLT* out) {
  FLT invq[4];
  quatgetreciprocal(invq, pose->Rot);

  FLT tvec[3];
  quatrotatevector(tvec, invq, pose->Pos);

  FLT t_pt[3];
  quatrotatevector(t_pt, pose->Rot, pt);
  for(int i = 0;i < 3;i++)
    t_pt[i] = t_pt[i] - tvec[i];
  
  
    FLT x = t_pt[0] / t_pt[2];
    FLT y = t_pt[1] / t_pt[2];

    double ang_x = atan(x);
    double ang_y = atan(y);

    const BaseStationData* bsd = &ctx->bsd[lighthouse]; 
    double phase[2];
    survive_calibration_options_config_apply(&config->phase, bsd->fcalphase, phase);
    double tilt[2];
    survive_calibration_options_config_apply(&config->tilt, bsd->fcaltilt, tilt);
    double curve[2];
    survive_calibration_options_config_apply(&config->curve, bsd->fcalcurve, curve);
    double gibPhase[2];
    survive_calibration_options_config_apply(&config->gibPhase, bsd->fcalgibpha, gibPhase);
    double gibMag[2];
    survive_calibration_options_config_apply(&config->gibMag, bsd->fcalgibmag, gibMag);

    out[0] =  ang_x + phase[0] + tan(tilt[0]) * y + curve[0] * y * y + gibf(config->gibUseSin, gibPhase[0] + ang_x) * gibMag[0];
    out[1] =  ang_y + phase[1] + tan(tilt[1]) * x + curve[1] * x * x + gibf(config->gibUseSin, gibPhase[1] + ang_y) * gibMag[1];
}

