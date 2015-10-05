#include <cstring>
#include "fftw++.h"

namespace fftwpp {

const double fftw::twopi=2.0*acos(-1.0);

// User settings:
unsigned int fftw::effort=FFTW_MEASURE;
const char *fftw::WisdomName="wisdom3.txt";
unsigned int fftw::maxthreads=1;
double fftw::testseconds=1.0; // Time limit for threading efficiency tests

fftw_plan (*fftw::planner)(fftw *f, Complex *in, Complex *out)=Planner;
fftw_plan (*fftw::rcplanner)(fftw *f, double *in, Complex *out)=rcPlanner;
fftw_plan (*fftw::crplanner)(fftw *f, Complex *in, double *out)=crPlanner;

const char *fftw::oddshift="Shift is not implemented for odd nx";
const char *inout=
  "constructor and call must be both in place or both out of place";

fft1d::Table fft1d::threadtable;
mfft1d::Table mfft1d::threadtable;
rcfft1d::Table rcfft1d::threadtable;
crfft1d::Table crfft1d::threadtable;
mrcfft1d::Table mrcfft1d::threadtable;
mcrfft1d::Table mcrfft1d::threadtable;
fft2d::Table fft2d::threadtable;

void LoadWisdom()
{
  static bool Wise=false;
  if(!Wise) {
    std::ifstream ifWisdom;
    ifWisdom.open(fftw::WisdomName);
    std::ostringstream wisdom;
    wisdom << ifWisdom.rdbuf();
    ifWisdom.close();
    fftw_import_wisdom_from_string(wisdom.str().c_str());
    Wise=true;
  }
}

void SaveWisdom()
{
  std::ofstream ofWisdom;
  ofWisdom.open(fftw::WisdomName);
  char *wisdom=fftw_export_wisdom_to_string();
  ofWisdom << wisdom;
  fftw_free(wisdom);
  ofWisdom.close();
}

fftw_plan Planner(fftw *F, Complex *in, Complex *out)
{
  LoadWisdom();
  fftw::effort |= FFTW_WISDOM_ONLY;
  fftw_plan plan=F->Plan(in,out);
  fftw::effort &= !FFTW_WISDOM_ONLY;
  if(!plan) {
    plan=F->Plan(in,out);
    SaveWisdom();
  }
  return plan;
}

fftw_plan rcPlanner(fftw *F, double *in, Complex *out)
{
  LoadWisdom();
  fftw::effort |= FFTW_WISDOM_ONLY;
  fftw_plan plan=F->rcPlan(in,out);
  fftw::effort &= !FFTW_WISDOM_ONLY;
  if(!plan) {
    plan=F->rcPlan(in,out);
    SaveWisdom();
  }
  return plan;
}

fftw_plan crPlanner(fftw *F, Complex *in, double *out)
{
  LoadWisdom();
  fftw::effort |= FFTW_WISDOM_ONLY;
  fftw_plan plan=F->crPlan(in,out);
  fftw::effort &= !FFTW_WISDOM_ONLY;
  if(!plan) {
    plan=F->crPlan(in,out);
    SaveWisdom();
  }
  return plan;
}

}
