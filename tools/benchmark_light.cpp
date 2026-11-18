// Cost of the light's per-tick work, 16r (before) against 17/11 (after): the 26 zone smoothers
//   (build: g++ -std=c++20 -O2 -I. tools/benchmark_light.cpp src/ui/atmosphere.cpp src/ui/gfx.cpp)
// (every poll) and the ring of 512 entries (every paint). The pixel loop and UpdateLayeredWindow
// are the same code in both, untouched.
#include "src/ui/ambient_ring.h"
#include <chrono>
#include <cstdio>
#include <vector>
// ---- 16r, copied: the two-stage smoother and the box-blurred sRGB ring
struct Old { double r=255,g=255,b=255,k=0,mr=255,mg=255,mb=255,mk=0; bool primed=false;
  bool step(Rgb t,double tk,double dt,double tau,bool organic){
    if(!primed){r=mr=t.r;g=mg=t.g;b=mb=t.b;k=mk=tk;primed=true;return true;}
    double a=1-std::exp(-dt/std::max(1e-3,tau)); double tr=t.r,tg=t.g,tb=t.b,tkk=tk;
    if(organic){mr+=(tr-mr)*a;mg+=(tg-mg)*a;mb+=(tb-mb)*a;mk+=(tkk-mk)*a;tr=mr;tg=mg;tb=mb;tkk=mk;}
    double nr=r+(tr-r)*a,ng=g+(tg-g)*a,nb=b+(tb-b)*a,nk=k+(tkk-k)*a;
    if(!organic){mr=nr;mg=ng;mb=nb;mk=nk;}
    bool moved=std::fabs(nr-r)+std::fabs(ng-g)+std::fabs(nb-b)>0.02||std::fabs(nk-k)>0.0005;
    r=nr;g=ng;b=nb;k=nk;return moved;}
  Rgb rgb()const{auto u=[](double v){return (uint8_t)std::lround(std::clamp(v,0.0,255.0));};return {u(r),u(g),u(b)};}};
static void old_ring(const Rgb* zone_,const double* light_,int ww,int wh,Rgb* out,uint8_t* oa){
  constexpr int kZones=26,kLut=512; double P=2.0*(ww+wh); double pos[kZones];
  for(int i=0;i<8;++i)pos[i]=(i+0.5)/8*ww; for(int i=0;i<5;++i)pos[8+i]=ww+(i+0.5)/5*wh;
  for(int i=0;i<8;++i)pos[13+i]=ww+wh+(i+0.5)/8*ww; for(int i=0;i<5;++i)pos[21+i]=2.0*ww+wh+(i+0.5)/5*wh;
  static double r[kLut],g[kLut],b[kLut],l[kLut];
  for(int k=0;k<kLut;++k){double p=(k+0.5)/kLut*P;int j=kZones-1;for(int z=0;z<kZones;++z)if(pos[z]<=p)j=z;int n=(j+1)%kZones;
    double a=pos[j],bpos=n==0?pos[0]+P:pos[n]; if(p<pos[0]){a=pos[kZones-1]-P;bpos=pos[0];}
    double t=std::clamp((p-a)/std::max(1.0,bpos-a),0.0,1.0);t=t*t*(3-2*t);
    r[k]=zone_[j].r+(zone_[n].r-zone_[j].r)*t;g[k]=zone_[j].g+(zone_[n].g-zone_[j].g)*t;b[k]=zone_[j].b+(zone_[n].b-zone_[j].b)*t;l[k]=light_[j]+(light_[n]-light_[j])*t;}
  static double tr[kLut],tg[kLut],tb[kLut],tl[kLut]; const int R=10;
  for(int k=0;k<kLut;++k){double sr=0,sg=0,sb=0,sl=0;for(int d=-R;d<=R;++d){int q=(k+d+kLut)%kLut;sr+=r[q];sg+=g[q];sb+=b[q];sl+=l[q];}
    tr[k]=sr/(2*R+1);tg[k]=sg/(2*R+1);tb[k]=sb/(2*R+1);tl[k]=sl/(2*R+1);}
  for(int k=0;k<kLut;++k){auto q=[](double v){return (uint8_t)std::lround(std::clamp(v,0.0,255.0));};out[k]={q(tr[k]),q(tg[k]),q(tb[k])};oa[k]=q(tl[k]*255);}}
static volatile unsigned sink;
template<class F> double ns(F f,int n){auto t0=std::chrono::steady_clock::now();for(int i=0;i<n;++i)f(i);return std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-t0).count()/n;}
int main(){
  Rgb za[26],zb[26]; double la[26],lb[26];
  for(int z=0;z<26;++z){za[z]={(uint8_t)(200-z*5),(uint8_t)(30+z*3),40};zb[z]={30,(uint8_t)(180-z*4),(uint8_t)(60+z*6)};la[z]=0.4+z*0.01;lb[z]=0.8-z*0.01;}
  Rgb out[512]; uint8_t oa[512];
  // the 26 smoothers through a cut (one poll each call)
  for(int rep=0;rep<3;++rep){
  Old o[26]; LightBreath n[26];
  for(int z=0;z<26;++z){o[z].step(za[z],la[z],0.033,0.3,false);n[z].step(za[z],la[z],0.033,0.2);}
  double t_old=ns([&](int i){unsigned s=0;for(int z=0;z<26;++z){s+=o[z].step((i/90)&1?za[z]:zb[z],(i/90)&1?la[z]:lb[z],0.033,0.3,true);s+=o[z].rgb().r;}sink=s;},200000);
  double t_new=ns([&](int i){unsigned s=0;for(int z=0;z<26;++z){s+=n[z].step((i/90)&1?za[z]:zb[z],(i/90)&1?la[z]:lb[z],0.033,0.2);s+=n[z].rgb().r;}sink=s;},200000);
  double r_old=ns([&](int i){za[i%26].g^=1;old_ring(za,la,1920,1080,out,oa);sink=out[i&511].r;},20000);
  double r_new=ns([&](int i){za[i%26].g^=1;ambient_ring(za,la,1920,1080,out,oa);sink=out[i&511].r;},20000);
  std::printf("zones step (26, per poll): before %.2f us  after %.2f us | ring (512, per paint): before %.1f us  after %.1f us\n",t_old/1000,t_new/1000,r_old/1000,r_new/1000);}
}
