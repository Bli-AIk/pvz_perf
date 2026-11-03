#include "hdr_reference.h"
#include "VulkanHdr.h"
#include <algorithm>
#include <array>
#include <cmath>

// Independent double-precision scalar evaluation of the legacy HDR equations.
// Test-only; the game never runs CPU tonemapping.
namespace HdrReference {
using RGB=std::array<double,3>;
double Saturate(double x) {return std::clamp(x,0.0,1.0);}
double Mix(double a,double b,double t) {return a+(b-a)*t;}
double PQ(double nits) {
    const double p=std::pow(std::max(nits,0.0)/10000.0,0.1593017578125);
    return std::pow((0.8359375+18.8515625*p)/(1+18.6875*p),78.84375);
}
double Spline(double value) {
    constexpr double x[]={0,.3,.6,.9,1},y[]={0,.470306,1.56789,6.24642,10};
    constexpr double e[]={1.56768,2.19488,5.92676,23.6637,37.5354};
    constexpr double slope[]={1.56768,3.65859,15.5951,37.5354};
    value=Saturate(value);
    const int i=value>=.9?3:value>=.6?2:value>=.3?1:0;
    const double dx=x[i+1]-x[i]+.000001,t=value-x[i];
    const double c3=(3*slope[i]-2*e[i]-e[i+1])/dx,c4=(e[i]+e[i+1]-2*slope[i])/(dx*dx);
    return y[i]+e[i]*t+c3*t*t+c4*t*t*t;
}
RGB Apply(RGB signal,const Sexy::VulkanHdrParams& p,bool linear) {
    RGB c{};
    for(int i=0;i<3;++i) {
        const double s=Saturate(signal[i]);
        c[i]=s<.04045?s/12.92:std::pow((s+.055)/1.055,2.4);
    }
    const double lum=.2125*c[0]+.7154*c[1]+.0721*c[2];
    const double cb=-.09991*c[0]-.33609*c[1]+.436*c[2],cr=.615*c[0]-.55861*c[1]-.05639*c[2];
    const double a=1000.0/p.reinhardPaperNits,k=(a+.0001)/(a-.9999),b=1/(a+1.0001);
    std::array<double,4> curve{};
    for(int i=0;i<4;++i) {
        const double x=i<3?c[i]:lum;
        const double rein=x>.5?b+x*k/(k-x):x/(1.0001-x);
        curve[i]=Mix(Spline(x)*100/p.reinhardPaperNits,rein,.5);
    }
    const RGB rec{curve[3]+1.280330*cr,curve[3]-.214820*cb-.380590*cr,curve[3]+2.127980*cb};
    const RGB lumaLerp{1.2,1.1,1};
    RGB col{};
    const double t=Saturate((p.peakNits-p.reinhardPaperNits)/(1000-p.reinhardPaperNits));
    for(int i=0;i<3;++i) {
        const double pres=curve[3]*c[i]/std::max(lum,.0001);
        const double luma=lumaLerp[i]*pres+(1-lumaLerp[i])*rec[i];
        const double out=.9*(.8*luma+.2*curve[i])+.1*c[i]*a;
        col[i]=Mix(c[i],out,t)*p.reinhardPaperNits/p.paperWhiteNits;
    }
    constexpr double matrix[3][3]={{.6274040,.3292860,.0433101},{.0690970,.9195400,.0113612},{.0163916,.0880132,.8955953}};
    constexpr double expand[3][3]={{.753845,.198593,.047562},{.045746,.941777,.012477},{-.001211,.017604,.983607}};
    const double maximum=*std::max_element(col.begin(),col.end());
    RGB output{};
    for(int i=0;i<3;++i) {
        double ref=0,normal=0,expanded=0;
        for(int j=0;j<3;++j){ref+=matrix[i][j]*c[j];normal+=matrix[i][j]*col[j];expanded+=expand[i][j]*col[j];}
        const double gate=Saturate((maximum-1)/(i==0?2.5:4.0));
        const double nits=Mix(ref*p.reinhardPaperNits/p.paperWhiteNits,Mix(normal,expanded,gate),p.strength)*p.paperWhiteNits;
        output[i]=linear?nits:PQ(nits);
    }
    if (linear) {
        constexpr double inverse[3][3]={{1.6604910172,-.5876472172,-.0728450165},
            {-.1245500243,1.1329005300,-.0083484533},{-.0181511207,-.1005786015,1.1187294311}};
        RGB rec709{};
        for(int i=0;i<3;++i) for(int j=0;j<3;++j) rec709[i]+=inverse[i][j]*output[j]/80.0;
        return rec709;
    }
    return output;
}
}
