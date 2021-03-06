#include <sys/utsname.h>
#include <unistd.h>
#include "micromegas.h"
#include "micromegas_aux.h"
//#include "micromegas_f.h"
#include "../CalcHEP_src/include/rootDir.h" 

#define ZeroCS 1E-40


char* CDM1=NULL, *CDM2=NULL,*aCDM1=NULL,*aCDM2=NULL;
aChannel* omegaCh=NULL;
aChannel* vSigmaTCh=NULL;
REAL *Qaddress=NULL;

static int do_err=0;
/*
static int NT;
static double *XX,*YY;
static void loadINTER(int N, double *x, double *y)
{ NT=N;XX=x;YY=y;}
double INTER(double x) { return polint3(x,NT,XX,YY);} 
*/




typedef  struct{ int virt,i3;double br,w[2];numout*cc23; int nTab; double *pcmTab; double *csTab;} processAuxRec;

typedef  processAuxRec* processAux;
static   processAux* code22Aux0, *code22Aux1,*code22Aux2;

static   processAux AUX;

double sWidth=0.01;

extern int  WIDTH_FOR_OMEGA;

static int neg_cs_flag;

static int NC=0;

static char ** inP;
static int  *  inAP;
static int  *  inG;
static int  *  inNum;
static int  *  feebleParticles;
static numout ** code22_0;
static numout ** code22_1;
static numout ** code22_2;

static int *inC0;    /* combinatoric coefficients  NCxNC*/
static int *inC1; 
static int *inC2;
static numout*cc23=NULL;

static REAL **inMassAddress;
static double *inMass;  /* masses */
static int *sort;
static int gaussInt=1;


static int LSP;

static double M1=0,M2=0;

static REAL pmass[6];
static int pdg[4];

#define XSTEP 1.1
static double eps=0.001; /* precision of integration */

static double MassCut;

static double s3f_;   /* to pass the Xf argument   */

static double T_;
static double Me_;


static int Z4ch( char *name)
{  if(name[0]!='~') return 0;
   if(name[1]!='~') return 1;
   return 2;
}



#define IMPROVE

/*
static double sigma23(double PcmIn)
{  int l,l_;
   double r;
   double brV1,MV1, MV2,wV1,wV2;
   int err;
   brV1=AUX[nsub22].br; 
   r=cs23(cc23,1,PcmIn,AUX[nsub22].i3,&err)/brV1/3.8937966E8;
   if(err) do_err=do_err|err;
   l=AUX[nsub22].virt;
   l_=5-l;  
   if(AUX[nsub22].w[l_-2])
   {  double m1,m2,sqrtS;
      m1=pmass[0];
      m2=pmass[1];
      MV1=pmass[l];
      MV2=pmass[l_];
      wV1=AUX[nsub22].w[l-2];
      wV2=AUX[nsub22].w[l_-2];
      sqrtS=sqrt(PcmIn*PcmIn+m1*m1) + sqrt(PcmIn*PcmIn+m2*m2); 
      if(wV1*wV2>0) r*=decayPcmW(sqrtS,MV1,MV2,wV1,wV2,0)/decayPcmW(sqrtS,MV1,MV2,wV1,0,0);    
   }   

   if(r<1.E-200) r=1.E-200;
   return log(r*PcmIn);
}  
*/ 


static  double sigma(double PcmIn)
{ double r; 

  if(AUX[nsub22].nTab>0 && PcmIn<=AUX[nsub22].pcmTab[AUX[nsub22].nTab-1])
  {  if(PcmIn<AUX[nsub22].pcmTab[0]) r= 0; else 
     { 
        r=exp(polint3(PcmIn,AUX[nsub22].nTab,AUX[nsub22].pcmTab,AUX[nsub22].csTab))/PcmIn;
     }  
  } 
  else
  {  if(kin22(PcmIn,pmass)) return 0.; 
     if(gaussInt) r=gauss(dSigma_dCos,-1.,1.,5); else 
     { int err;
       r=simpson(dSigma_dCos,-1.,1.,0.3*eps,&err);
       if(err) {do_err= do_err|err;  printf("error in simpson omega.c line 130\n");}
     }   
     if(r<0) { neg_cs_flag=1;r=0;}

     if((VZdecay||VWdecay) && (AUX[nsub22].w[0] || AUX[nsub22].w[1] )) 
     { double f; 
       double sqrtS=sqrt(PcmIn*PcmIn+pmass[0]*pmass[0]) + sqrt(PcmIn*PcmIn+pmass[1]*pmass[1]); 
       if( sqrtS-pmass[2]-pmass[3] < 15*(AUX[nsub22].w[0]+AUX[nsub22].w[1]))   
         f=decayPcmW(sqrtS,pmass[2],pmass[3],AUX[nsub22].w[0],AUX[nsub22].w[1],5)/decayPcm(sqrtS,pmass[2],pmass[3]);     
       else   
       {   f=1; 
           if(AUX[nsub22].w[0]) f-= AUX[nsub22].w[0]/pmass[2]/M_PI;
           if(AUX[nsub22].w[1]) f-= AUX[nsub22].w[1]/pmass[3]/M_PI;
       }
       r*=f;
     }
  }  
#ifdef IMPROVE
  improveCrossSection(pdg[0],pdg[1],pdg[2],pdg[3],PcmIn,&r);
#endif  
  return r;
}  



static double geffDM(double T)
{ double sum=0; int l;

  for(l=0;l<NC;l++) if(!feebleParticles[oddPpos[sort[l]]]) 
  { int k=sort[l];
    double A=Mcdm/T*(inMass[k]-Mcdm)/Mcdm   ;
    if(A>15 || Mcdm +inMass[k] > MassCut) return sum;
    sum+=inG[k]*pow(inMass[k]/Mcdm,1.5)*exp(-A)*K2pol(1/(Mcdm/T+A));
  }
  return sum;
}

static double y_pass;


static double weight_integrand(double s3)
{  double x,gf;
   double T,heff,geff;

   if(s3==0.) return 0;
   T=T_s3(s3);
   heff=hEff(T);
   geff=gEff(T);
   gf=geffDM(T);
   return K1pol(T/(Mcdm*y_pass))*exp((1/T-1/T_)*(2-y_pass)*Mcdm)*sqrt(Mcdm/T)*heff/sqrt(geff)/(gf*gf*s3);
}


static double weightBuff_x[1000];
static double weightBuff_y[1000];
static int inBuff=0;


static double weight(double y)
{ int i;
  double w;
  for(i=0;i<inBuff;i++) if(y==weightBuff_x[i]) { return weightBuff_y[i]; }
  y_pass=y;
  { int err;
    w=  simpson(weight_integrand,0.,s3f_,0.3*eps,&err);
    if(err) {do_err=do_err|err; printf("error in simpson omega.c line 195\n");}
  }  
  if(inBuff<1000){weightBuff_x[inBuff]=y; weightBuff_y[inBuff++]=w;}
  return w;
}

static int exi;

static double s_integrand( double u)
{  double y,sv_tot,w;
   double Xf_1;
   double ms,md,sqrtS,PcmIn,res0;
   
   if(u==0. || u==1.) return 0.;
   
   long double u_=u,z=u_*(2-u_);
      
//   long double u_=u,z=(1-u_)*(1+u_);
   sqrtS=M1+M2-3*T_*logl(z);
   y=sqrtS/Mcdm;
   ms = M1 + M2;  if(ms>=sqrtS)  return 0;
   md = M1 - M2;
   PcmIn = sqrt((sqrtS-ms)*(sqrtS+ms)*(sqrtS-md)*(sqrtS+md))/(2*sqrtS);
   sv_tot=sigma(PcmIn);         
   res0=sqrt(2*y/M_PI)*y*(PcmIn*PcmIn/(Mcdm*Mcdm))*sv_tot*6*(1-u)*z*z; 
   if(exi) { return res0*weight(sqrtS/Mcdm); } else return  res0*K1pol(T_/sqrtS)*sqrt(Mcdm/T_);
}


static  double m2v(double m) { long double  e=expl(((M1+M2 -m)/T_)/3); if(e>=1) return e; else return e/(1+sqrtl(1-e));} 


typedef struct vGridStr
{  int n;
   double v[100];
   int pow[100];  
}  vGridStr;

static double v_max=1,v_min=0;

static vGridStr   makeVGrid(double mp,double wp)
{
  vGridStr grd;

  int pow_[6]={7,  3,  4, 4, 3,  5};
  double c[5]={ -10,-3, 0, 3, 10};

  int n,j,jmax=4;
  
  grd.v[0]=v_min;
  for(j=jmax,n=1 ;j>=0;j--)
  { double v=m2v(mp+c[j]*wp);
    if(isfinite(v) && v>v_min && v < v_max) 
    {  
      grd.v[n]=v;
      grd.pow[n-1]=pow_[j+1];
      grd.pow[n  ]=pow_[j];
      n++;
    }  
  }
  grd.v[n]=v_max;
  if(n==1) grd.pow[0]=5;
  grd.n=n;
  return grd;
}

static vGridStr   makeVGrid2(double mp,double wp)
{
  vGridStr grd;

  int pow_[6]={2, 4, 2};
  double c[5]={-3, 3};

  int n,j,jmax=1;
  
  grd.v[0]=v_min;
  for(j=jmax,n=1 ;j>=0;j--)
  { double v=m2v(mp+c[j]*wp);
    if(isfinite(v) && v>v_min && v < v_max) 
    { 
      grd.v[n]=v;
      grd.pow[n-1]=pow_[j+1];
      grd.pow[n  ]=pow_[j];
      n++;
    }  
  }
  grd.v[n]=v_max;
  if(n==1) grd.pow[0]=5;
  grd.n=n;
  return grd;
}



static vGridStr  crossVGrids(vGridStr * grid1, vGridStr * grid2)
{ vGridStr grid;
  int n=0,i1=1,i2=1,i;
  grid.v[0]=v_min;
  n=1;
  while(i1<=grid1->n && i2<=grid2->n)
  { double d1= grid1->pow[i1-1]/(grid1->v[i1]-grid1->v[i1-1]);
    double d2= grid2->pow[i2-1]/(grid2->v[i2]-grid2->v[i2-1]);
    double d = ( d1>d2? d1:d2);
    int m=(grid1->pow[i1-1] > grid2->pow[i2-1]? grid1->pow[i1-1]:grid2->pow[i2-1]);

    if(grid1->v[i1] < grid2->v[i2]) { grid.v[n]=grid1->v[i1++];}
    else                            { grid.v[n]=grid2->v[i2];
                                      if(grid1->v[i1]==grid2->v[i2])i1++; 
                                      i2++;
                                    }  
                                        
    grid.pow[n-1]=0.999+d*(grid.v[n]-grid.v[n-1]);

    if(grid.pow[n-1]>m)   grid.pow[n-1]=m;
    if(grid.pow[n-1]<2)   grid.pow[n-1]=2;

    n++;
  }
  grid.n=n-1;
  for(i=0;i<grid.n;i++) if(grid.v[i+1]-grid.v[i]>0.4 && grid.pow[i]<4)  grid.pow[i]=4;
  return grid;
}

static void printVGrid(vGridStr gr)
{
   printf("gr.n=%d\n", gr.n);
   int i;
   for(i=0;i<gr.n;i++) printf("      %d      ", gr.pow[i]);
   printf("\n");
   for(i=0;i<=gr.n;i++) printf(" %e", gr.v[i]);
   printf("\n");
}




static int testSubprocesses(void)
{
  static int first=1;
  int err,k1,k2,i,j;
  CDM1=CDM2=NULL;
  Mcdm=Mcdm1=Mcdm2=0;
   
  if(first)
  {
    first=0;
    if(createTableOddPrtcls())
    { printf("The model contains uncoupled odd patricles\n"); exit(10);}
    
    for(i=0,NC=0;i<Nodd;i++,NC++) 
        if(strcmp(OddPrtcls[i].name,OddPrtcls[i].aname))NC++;
    oddPpos=(int*)malloc(NC*sizeof(int));        
    inP=(char**)malloc(NC*sizeof(char*));
    inAP=(int*)malloc(NC*sizeof(int));
    inG=(int*)malloc(NC*sizeof(int));
    inMassAddress=(REAL**)malloc(NC*sizeof(REAL*));
    inMass=(double*)malloc(NC*sizeof(double));
    inNum= (int*)malloc(NC*sizeof(int));
    if(!feebleParticles) 
    {  feebleParticles=(int*)malloc(sizeof(int)*nModelParticles);
       for(i=0;i<nModelParticles;i++) feebleParticles[i]=0;
    }
    sort=(int*)malloc(NC*sizeof(int));

    code22_0 = (numout**)malloc(NC*NC*sizeof(numout*));
    code22_1 = (numout**)malloc(NC*NC*sizeof(numout*));
    code22_2 = (numout**)malloc(NC*NC*sizeof(numout*));
            
    inC0=(int*)malloc(NC*NC*sizeof(int)); 
    inC1=(int*)malloc(NC*NC*sizeof(int));
    inC2=(int*)malloc(NC*NC*sizeof(int));
            
    code22Aux0=(processAux*) malloc(NC*NC*sizeof(processAux));
    code22Aux1=(processAux*) malloc(NC*NC*sizeof(processAux));
    code22Aux2=(processAux*) malloc(NC*NC*sizeof(processAux));
    
    for(i=0,j=0;i<Nodd;i++)
    {  
       inP[j]=OddPrtcls[i].name;
       for(int k=0;k<nModelParticles;k++) if(strcmp(inP[j],ModelPrtcls[k].name)==0 || strcmp(inP[j],ModelPrtcls[k].aname)==0) 
       { oddPpos[j]=k; break;}
       inNum[j]=OddPrtcls[i].NPDG;
       inG[j]=(OddPrtcls[i].spin2+1)*OddPrtcls[i].cdim;
       if(strcmp(OddPrtcls[i].name,OddPrtcls[i].aname))
       {
         inAP[j]=j+1;
         j++;
         oddPpos[j]=oddPpos[j-1];       
         inP[j]=OddPrtcls[i].aname;
         inG[j]=inG[j-1];
         inAP[j]=j-1;
         inNum[j]=-OddPrtcls[i].NPDG;
       } else inAP[j]=j;
       j++;
     }

    for(i=0;i<NC;i++) sort[i]=i;
    for(k1=0;k1<NC;k1++) for(k2=0;k2<NC;k2++) inC0[k1*NC+k2]=-1;
    for(k1=0;k1<NC;k1++) for(k2=0;k2<NC;k2++) if(inC0[k1*NC+k2]==-1)
    {  int kk1=inAP[k1];
       int kk2=inAP[k2];
       inC0[k1*NC+k2]=1;
       if(inC0[k2*NC+k1]==-1)   {inC0[k2*NC+k1]=0;   inC0[k1*NC+k2]++;}
       if(inC0[kk1*NC+kk2]==-1) {inC0[kk1*NC+kk2]=0; inC0[k1*NC+k2]++;}
       if(inC0[kk2*NC+kk1]==-1) {inC0[kk2*NC+kk1]=0; inC0[k1*NC+k2]++;}
    }

    for(k1=0;k1<NC;k1++) for(k2=0;k2<NC;k2++) 
    { int L=k1*NC+k2;
       code22_0[L]=NULL;
       code22_1[L]=NULL;

       code22_2[L]=NULL;
       code22Aux0[L]=NULL;
       code22Aux1[L]=NULL;
       code22Aux2[L]=NULL;
       inC1[L]=inC0[L];
       inC2[L]=inC0[L];
    }    

    for(i=0,j=0;i<Nodd;i++)
    {
       inMassAddress[j]=varAddress(OddPrtcls[i].mass);
       if(!inMassAddress[j]) 
       { if(strcmp(OddPrtcls[i].mass ,"0")==0)
         { printf("Error: odd particle '%s' has zero mass.\n",OddPrtcls[i].name);
           exit(5);
         }  
         printf(" Model is not self-consistent:\n "
                " Mass identifier '%s' for particle '%s' is absent  among parameetrs\n",OddPrtcls[i].mass, OddPrtcls[i].name);
         exit(5);
       }

       if(strcmp(OddPrtcls[i].name,OddPrtcls[i].aname))
       {
         j++;
         inMassAddress[j]=inMassAddress[j-1];
       }
       j++;
    }
    for(Qaddress=NULL,i=0;i<nModelVars;i++) if(strcmp(varNames[i],"Q")==0) Qaddress=varValues+i;
  }

  cleanDecayTable(); 
  if(Qaddress) *Qaddress=100;
 
  err=calcMainFunc();
  if(err>0) return err;
   
  if(Nodd==0) { printf("No odd particles in the model\n"); return -1; }

  Mcdm=fabs(*(inMassAddress[0]));
  for(i=0;i<NC;i++) 
  { inMass[i]=fabs(*(inMassAddress[i]));
    if(Mcdm>inMass[i]) Mcdm=inMass[i];
  }
  
  if(Qaddress) 
  { *Qaddress=2*Mcdm;
     err=calcMainFunc();
     if(err>0) return err;
  }
            
  GGscale=2*Mcdm/3;

  for(i=0; i<NC-1;)
   {  int i1=i+1;
      if(inMass[sort[i]] > inMass[sort[i1]])
      { int c=sort[i]; sort[i]=sort[i1]; sort[i1]=c;
        if(i) i--; else i++;
      } else i++;
   }

 
  for(i=0;i<NC;i++) 
  {
    if(Z4ch(inP[i])==1) { if(!CDM1) { Mcdm1=inMass[i]; CDM1=inP[i];} else if(Mcdm1>inMass[i]) { Mcdm1=inMass[i];CDM1=inP[i];} }
    if(Z4ch(inP[i])==2) { if(!CDM2) { Mcdm2=inMass[i]; CDM2=inP[i];} else if(Mcdm2>inMass[i]) { Mcdm2=inMass[i];CDM2=inP[i];} }
  }
  
  if(CDM1 && CDM2) for(i=0;i<Nodd;i++) if(Z4ch(OddPrtcls[i].name) != Z4ch(OddPrtcls[i].aname))
  { if(Mcdm1>Mcdm2) { Mcdm1=Mcdm2; CDM1=CDM2;} 
    CDM2=NULL;
    Mcdm2=0;
  }  
  if(CDM1){for(i=0;i<Nodd;i++) if(OddPrtcls[i].name==CDM1) { aCDM1=OddPrtcls[i].aname; break;}}
  else aCDM1=NULL;
  if(CDM2){for(i=0;i<Nodd;i++) if(OddPrtcls[i].name==CDM2) { aCDM2=OddPrtcls[i].aname; break;}}
  else aCDM2=NULL;
  
            
  if(CDM1){strcpy(CDM1_,CDM1); i=strlen(CDM1); } else i=0;
  for(;i<20;i++) CDM1_[i]=' ';
  if(CDM2){strcpy(CDM2_,CDM2); i=strlen(CDM2); } else i=0;
  for(;i<20;i++) CDM2_[i]=' ';

  LSP=sort[0];
  Mcdm=inMass[LSP];

  for(k1=0;k1<NC;k1++)  for(k2=0;k2<NC;k2++) 
  {  if(code22_0[k1*NC+k2]) code22_0[k1*NC+k2]->init=0;
     if(code22_1[k1*NC+k2]) code22_1[k1*NC+k2]->init=0;
     if(code22_2[k1*NC+k2]) code22_2[k1*NC+k2]->init=0;
  }             
  cleanDecayTable();

  for(k1=0;k1<NC;k1++) for(k2=0;k2<NC;k2++) if( code22_0[k1*NC+k2])
  { int  nprc=code22_0[k1*NC+k2]->interface->nprc;
    processAux prc=code22Aux0[k1*NC+k2];
    int n;
    for(n=0;n<=nprc;n++)
    { 
        prc[n].w[0]=0;
        prc[n].w[1]=0;
        prc[n].virt=0;
        prc[n].i3=0;  
        prc[n].br=0;  
        prc[n].cc23=NULL;
        prc[n].virt=0; 
        
        prc[n].nTab=0;   
        if(prc[n].pcmTab) { free(prc[n].pcmTab); prc[n].pcmTab=NULL;}
        if(prc[n].csTab)  { free(prc[n].csTab);  prc[n].csTab=NULL; }  
    }     
  }

  for(k1=0;k1<NC;k1++) for(k2=0;k2<NC;k2++) if( code22_1[k1*NC+k2])
  { int  nprc=code22_1[k1*NC+k2]->interface->nprc;
    processAux prc=code22Aux1[k1*NC+k2];
    int n;
    for(n=0;n<=nprc;n++)
    { 
        prc[n].w[0]=0;
        prc[n].w[1]=0;
        prc[n].virt=0;
        prc[n].i3=0;  
        prc[n].br=0;  
        prc[n].cc23=NULL;
        prc[n].virt=0; 
        
        prc[n].nTab=0;   
        if(prc[n].pcmTab) { free(prc[n].pcmTab); prc[n].pcmTab=NULL;}
        if(prc[n].csTab)  { free(prc[n].csTab);  prc[n].csTab=NULL; }  
    }     
  }

  
  return 0;
}


int toFeebleList(char*name)
{ int i;
  if(!feebleParticles) 
  {  feebleParticles=(int*)malloc(sizeof(int)*nModelParticles);
     for(i=0;i<nModelParticles;i++) feebleParticles[i]=0;
  }
  if(name==NULL) { for(i=0;i<nModelParticles;i++) feebleParticles[i]=0;  return 0;}
  char name_[20];
  strcpy(name_,name);
  trim(name_);
  for(i=0;i<nModelParticles;i++) 
  if(strcmp(name_,ModelPrtcls[i].name)==0 || strcmp(name_,ModelPrtcls[i].aname)==0 ) { feebleParticles[i]=1;  return 0;}
  printf(" particle \"%s\" is out of particle list\n", name_); return 1;
}


int isFeeble(char*name)
{ 
  int p=pTabPos(name); 
  if(p==0) return 0; 
  return feebleParticles[abs(p)-1];  
}
/*
double aWidth(char * pName)
{  txtList LL;  
   return pWidth(pName,&LL);
}
*/
 
int sortOddParticles(char * lsp)
{ int i,err,nprc;

  nprc=sysconf(_SC_NPROCESSORS_ONLN);
  if(nPROCSS>nprc) nPROCSS=nprc;
  err=loadHeffGeff(NULL);

  if(!modelNum)
  {
    int i,k,L;
    struct utsname buff;

    L=strlen(WORK);
    modelDir=malloc(L+15);  sprintf(modelDir,"%s/models",WORK);
    modelNum=1;
  
    calchepDir=malloc(strlen(rootDir)+1);strcpy(calchepDir,rootDir);
    uname(&buff);
    compDir=malloc(strlen(WORK)+strlen(buff.nodename)+25);  
    strcpy(compDir,WORK);
    sprintf(compDir+strlen(compDir),"/_%s_%d_",buff.nodename,getpid());
      
    libDir=malloc(L+15); sprintf(libDir,"%s/so_generated",WORK);  
  }
  
  
  if(omegaCh)   {free(omegaCh);   omegaCh=NULL;}
  if(vSigmaTCh) {free(vSigmaTCh); vSigmaTCh=NULL;}
  //if(vSigmaCh)  {free(vSigmaCh);  vSigmaCh=NULL; } 
  err=testSubprocesses();
  if(err)
  { 
    if(lsp)
    {
      if(err>0) {strcpy(lsp,varNames[err]); printf("can not calculate parameter %s\n",varNames[err]);}
       else strcpy(lsp,"Nodd=0");
       printf("sortOddparticles err=%d\n",err);
    }    
    return err;
  }

  if(lsp) strcpy(lsp,inP[LSP]);
  return 0;
}


char * OddParticles(int mode)
{ 
  static char * out[3]={NULL,NULL,NULL};
  int i,len;
  if(mode<0||mode>2) return NULL;
  
  if(out[mode]) return out[mode];

  for(i=0,len=0;i<Nodd;i++) 
  { if(mode==0 || Z4ch(OddPrtcls[i].name)==mode) 
    len+=strlen(OddPrtcls[i].name)+1;
    if(strcmp(OddPrtcls[i].name,OddPrtcls[i].aname))
    len+=strlen(OddPrtcls[i].aname)+1;
  }
  out[mode]=malloc(len); out[mode][0]=0;

  for(i=0;i<Nodd;i++) if(mode==0 || Z4ch(OddPrtcls[i].name)==mode)
  { 
    if(out[mode][0]) strcat(out[mode],","); 
    strcat(out[mode],OddPrtcls[i].name);
    if(strcmp(OddPrtcls[i].name,OddPrtcls[i].aname))
    { strcat(out[mode],",");
      strcat(out[mode],OddPrtcls[i].aname);
    } 
  }
  return out[mode];
}


char * EvenParticles(void)
{ 
  static char * out=NULL;
  int i,len;
  if(out) return out;
  for(i=0,len=0;i<nModelParticles;i++) if(ModelPrtcls[i].name[0]!='~')
  {
    len+=strlen(ModelPrtcls[i].name)+1;
    if(strcmp(ModelPrtcls[i].name,ModelPrtcls[i].aname))
    len+=strlen(ModelPrtcls[i].aname)+1;
  }
  out=malloc(len); out[0]=0;
  for(i=0;i<nModelParticles;i++)if(ModelPrtcls[i].name[0]!='~') 
  { 
    if(out[0]) strcat(out,",");
    strcat(out,ModelPrtcls[i].name);
    if(strcmp(ModelPrtcls[i].name,ModelPrtcls[i].aname))
    { strcat(out,",");
      strcat(out,ModelPrtcls[i].aname);
    } 
  }
  return out;
}


static int  new_code(int k1,int k2, int ch)
{
   char lib[40];
   char process[4000];
   char lib1[12],lib2[12];
   numout*cc;
  
   pname2lib(inP[k1],lib1);
   pname2lib(inP[k2],lib2); 
   sprintf(lib,"omg_%s%s",lib1,lib2);
   switch(ch)
   { case  0: sprintf(process,"%s,%s->AllEven,1*x{%s",inP[k1],inP[k2],EvenParticles()); break;
     case  1: sprintf(process,"%s,%s->AllOdd1,AllOdd1{%s",inP[k1],inP[k2],OddParticles(1));
             strcat(lib,"_1"); break;
     case -1: sprintf(process,"%s,%s->AllOdd2,AllOdd2{%s",inP[k1],inP[k2],OddParticles(2));
             strcat(lib,"_2"); break;
     case  2: sprintf(process,"%s,%s->AllOdd1,AllOdd2{%s{%s",inP[k1],inP[k2],OddParticles(1),OddParticles(2));
                  strcat(lib,"_12"); break;                        
   } 
   cc=getMEcode(0,ForceUG,process,NULL,NULL,lib);
   if(cc) 
   {  int nprc,n;
      processAux prc;
      *(cc->interface->twidth)=1;
      switch(ch)
      { case  0:  code22_0[k1*NC+k2]=cc; break;
        case  1: 
        case -1:  code22_1[k1*NC+k2]=cc; break;
        case  2:  code22_2[k1*NC+k2]=cc; break;
      }  
      nprc=cc->interface->nprc;
      prc=(processAux)malloc((nprc+1)*sizeof(processAuxRec));
      
      switch(ch)
      { case  0: code22Aux0[k1*NC+k2]=prc; break;
        case  1: 
        case -1: code22Aux1[k1*NC+k2]=prc; break;
        case  2: code22Aux2[k1*NC+k2]=prc; break;
      }  
      for(n=0;n<=nprc;n++)
      { 
        prc[n].w[0]=0;
        prc[n].w[1]=0;
        prc[n].virt=0;
        prc[n].i3=0; 
        prc[n].br=0; 
        prc[n].cc23=NULL;
        prc[n].nTab=0;   
        prc[n].pcmTab=NULL;
        prc[n].csTab=NULL;                          
      }   
   } else 
   {  switch(ch)
      { case  0:  inC0[k1*NC+k2]=0; break;
        case  1:  
        case -1:  inC1[k1*NC+k2]=0; break;
        case  2:  inC2[k1*NC+k2]=0; break;
      }
   }   
   return 0;
}


static int Ntab=0;
static double*Ttab=NULL;


static double *vs1100T = NULL;
static double *vs1120T = NULL;
static double *vs1122T = NULL;
static double *vs1210T = NULL;
static double *vs2200T = NULL;
static double *vs2211T = NULL;

static double *vs1110T = NULL;
static double *vs2220T = NULL;
static double *vs1112T = NULL;
static double *vs1222T = NULL;
static double *vs1220T = NULL;
static double *vs2210T = NULL;
static double *vs2221T = NULL;
static double *vs1211T = NULL;
//static double *TCoeff  = NULL;
static double *Y1T=NULL;
static double *Y2T=NULL;

double vs1120F(double T){ return  3.8937966E8*exp(polint3(T,Ntab,Ttab,  vs1120T));}
double vs2200F(double T){ return  3.8937966E8*exp(polint3(T,Ntab,Ttab,  vs2200T));}
double vs1100F(double T){ return  3.8937966E8*exp(polint3(T,Ntab,Ttab,  vs1100T));}
double vs1210F(double T){ return  3.8937966E8*exp(polint3(T,Ntab,Ttab,  vs1210T));}
double vs1122F(double T){ return  3.8937966E8*exp(polint3(T,Ntab,Ttab,  vs1122T));}
double vs2211F(double T){ return  3.8937966E8*exp(polint3(T,Ntab,Ttab,  vs2211T));}

double vs1110F(double T){ return  3.8937966E8*exp(polint3(T,Ntab,Ttab,  vs1110T));}
double vs2220F(double T){ return  3.8937966E8*exp(polint3(T,Ntab,Ttab,  vs2220T));}
double vs1112F(double T){ return  3.8937966E8*exp(polint3(T,Ntab,Ttab,  vs1112T));}
double vs1222F(double T){ return  3.8937966E8*exp(polint3(T,Ntab,Ttab,  vs1222T));}
double vs1220F(double T){ return  3.8937966E8*exp(polint3(T,Ntab,Ttab,  vs1220T));}
double vs2210F(double T){ return  3.8937966E8*exp(polint3(T,Ntab,Ttab,  vs2210T));}
double vs2221F(double T){ return  3.8937966E8*exp(polint3(T,Ntab,Ttab,  vs2221T));}
double vs1211F(double T){ return  3.8937966E8*exp(polint3(T,Ntab,Ttab,  vs1211T));}

//double TCoeffF(double T) { return polint3(T,Ntab,Ttab, TCoeff);}

double dY1F(double T){ return polint3(T,Ntab-1,Ttab, Y1T) ;}
double dY2F(double T){ return polint3(T,Ntab-1,Ttab, Y2T) ;}

double Y1F(double T){ return  dY1F(T)+Yeq1(T);}
double Y2F(double T){ return  dY2F(T)+Yeq2(T);}



   
static void gaussC2(double * c, double * x, double * f)
{
  double  A[2][2];
  int i,j;
  double det;
  double B[2];
    
  for(i=0;i<2;i++)
  { int l=1; for(j=0;j<2;j++) {A[i][j]=l*c[i+j]; l=-l;}  
     B[i]=l*c[2+i];
  }
  
  det=A[0][0]*A[1][1] - A[0][1]*A[1][0];
  
  f[0]= ( B[0]*A[1][1]-B[1]*A[0][1])/det;
  f[1]= (-B[0]*A[1][0]+B[1]*A[0][0])/det;
  
  det=sqrt(f[0]+f[1]*f[1]/4.);
   
  x[0]= -f[1]/2.-det;
  x[1]= -f[1]/2.+det;
 
  for(i=0;i<2;i++) { B[i]=c[i]; A[0][i]=1; }
  for(j=0;j<2;j++)   A[1][j]=A[0][j]*x[j];

  det= A[0][0]*A[1][1] - A[0][1]*A[1][0];
  
  f[0]= ( B[0]*A[1][1]-B[1]*A[0][1])/det;
  f[1]= (-B[0]*A[1][0]+B[1]*A[0][0])/det;
} 



static double aRate(double X, int average,int Fast, double * alpha, aChannel ** wPrc,int *NPrc)
{
  double Sum=0.;
  double Sum1=0;
  int i,l1,l2;
  int nPrc=0;
  char* pname[5];
  vGridStr vgrid,vgrid1;  
  double MassCutOut=MassCut+Mcdm*log(1000.)/X;
  double Msmall,Mlarge;

  int nPrcTot=0;
  if(MassCutOut<Mcdm*(2+10/X)) MassCutOut=Mcdm*(2+10/X); 
  WIDTH_FOR_OMEGA=1;

  T_=Mcdm/X;
  s3f_ = s3_T(T_);
  exi=average;

  if(wPrc) *wPrc=NULL;

  for(l1=0;l1<NC;l1++)  if(!feebleParticles[oddPpos[sort[l1]]] && Mcdm+inMass[sort[l1]]<MassCut )
  for(l2=0;l2<NC;l2++)  if(!feebleParticles[oddPpos[sort[l2]]] && inMass[sort[l1]]+inMass[sort[l2]]<MassCut) 
  {    
    double Sumkk=0.;
    double Sum1kk=0;
    double x[2],f[2];
    double factor;
    int kk,k1=sort[l1],k2=sort[l2];
    CalcHEP_interface * CI;

    if(inC0[k1*NC+k2]<=0) continue;
    if(code22_0[k1*NC+k2]==NULL) new_code(k1,k2,0);
    if(inC0[k1*NC+k2]<=0) continue;
    if(!code22_0[k1*NC+k2]->init)
    {
      if(Qaddress && *Qaddress!=inMass[k1]+inMass[k2]) 
      { *Qaddress=inMass[k1]+inMass[k2];
         calcMainFunc();
      }       
      if(passParameters(code22_0[k1*NC+k2])) {FError=1; WIDTH_FOR_OMEGA=0;  return -1;}
      code22_0[k1*NC+k2]->init=1;
    }
    if(wPrc)
    {  nPrcTot+=code22_0[k1*NC+k2]->interface->nprc;
       *wPrc=(aChannel*)realloc(*wPrc,sizeof(aChannel)*(nPrcTot+1));
    }
    //squared matrix element
    sqme22=code22_0[k1*NC+k2]->interface->sqme;
    inBuff=0;

    M1=inMass[k1];
    M2=inMass[k2];


    Msmall=M1>M2? M1-Mcdm*(1-sWidth): M2-Mcdm*(1-sWidth);
    Mlarge=M1>M2? M2+Mcdm*(1-sWidth): M1+Mcdm*(1-sWidth);

    v_min=m2v(MassCutOut);
    if(v_min<1E-200) v_min=1E-200; 

    factor=inC0[k1*NC+k2]*inG[k1]*inG[k2]*exp(-(M1+M2 -2*Mcdm)/T_);
    CI=code22_0[k1*NC+k2]->interface;
    AUX=code22Aux0[k1*NC+k2];
    for(nsub22=1; nsub22<= CI->nprc;nsub22++,nPrc++)
    { double smin;
      double a=0;
      double K=0;
      for(i=0;i<4;i++) pname[i]=CI->pinf(nsub22,i+1,pmass+i,pdg+i);
      if(wPrc) 
      { (*wPrc)[nPrc].weight=0;
        for(i=0;i<4;i++) (*wPrc)[nPrc].prtcl[i]=pname[i];
        (*wPrc)[nPrc].prtcl[4]=NULL;
      }

      if(pmass[0]<Mcdm/2 || pmass[1]<Mcdm/2) continue; 
      
      smin=pmass[2]+pmass[3];
      cc23=NULL;
      
      if(VZdecay||VWdecay)
      {  int l,l_,nVV;        

         if(!AUX[nsub22].virt )  for(l=2;l<4;l++) if(pdg[l]==21 ||pdg[l]==22) { AUX[nsub22].virt=-1; break;}
         
         if(!AUX[nsub22].virt)
         {  int vd[4]={0,0,0,0};
            int c_a =  (pmass[0]>Mcdm) || (pmass[1]>Mcdm);

            if(c_a){ for(l=2;l<4;l++) if((pdg[l]==23 && VZdecay>1)   || (abs(pdg[l])==24 && VWdecay>1)) vd[l]=1;} 
            else    for(l=2;l<4;l++)
            { 
            
              if((pdg[l]==23 && VZdecay)     || (abs(pdg[l])==24 && VWdecay)) vd[l]=1;
            } 

            for(l=2;l<4;l++) if(vd[l]) break; 
            if(l<4)
            {  l_=5-l; 
               if(vd[l_])
               { nVV=2;
                 if(pmass[l_]>pmass[l]) { l=l_; l_=5-l;}
               } else nVV=1; 
               AUX[nsub22].virt=l;  
               AUX[nsub22].w[l-2]=pWidth(pname[l],NULL);
               if(abs(pdg[l_])>16 && pmass[l_]> 2) AUX[nsub22].w[l_-2]=pWidth(pname[l_],NULL);
               if(AUX[nsub22].w[l_-2] < 0.1) AUX[nsub22].w[l_-2]=0;
            } else  AUX[nsub22].virt=-1;
         }

      }
      
      
//if(cc23)  printf("23  %s %s -> %s %s\n", pname[0],pname[1],pname[2],pname[3]);
    
     
//if(abs(pdg[2])!=24 && abs(pdg[3])!=24) continue; 

      if(cc23==NULL) 
      {                               
         if( (pmass[2]>Mlarge && pmass[3]<Msmall)
           ||(pmass[3]>Mlarge && pmass[2]<Msmall))
            { *(CI->twidth)=1; *(CI->gtwidth)=1;} else { *(CI->twidth)=0; *(CI->gtwidth)=0;}
      } 
      *(CI->gswidth)=0;

//*(CI->twidth)=0; *(CI->gtwidth)=0;

      if(smin > pmass[0]+pmass[1])
      { 
        if((pmass[0]!=M1 || pmass[1]!=M2)&&(pmass[0]!=M2 || pmass[1]!=M1))
        { double ms=pmass[0]+pmass[1];
          double md=pmass[0]-pmass[1];
          double Pcm=sqrt((smin-ms)*(smin+ms)*(smin-md)*(smin+md))/(2*smin);
          smin=sqrt(M1*M1+Pcm*Pcm)+sqrt(M2*M2+Pcm*Pcm);
        }      
      }

      if(pmass[0]+pmass[1]> smin) smin=pmass[0]+pmass[1];
      v_max=m2v(smin); 
//printf("v_min=%E v_max=%E  smin=%e\n", v_min,v_max,smin);      
      if(v_max<=v_min) continue; 
            
repeat:
      neg_cs_flag=0;
       
      if(Fast==0)
      { int err; 
        a=simpson(s_integrand,v_min,v_max,eps,&err);
        if(err) { do_err=do_err|err; printf("error in simpson omega.c line 1004\n");}
      } else
      {
          int isPole=0;
          char * s;
          int m,w,n;
          double mass,width;

          for(n=1;(s=code22_0[k1*NC+k2]->interface->den_info(nsub22,n,&m,&w,NULL));n++)
          if(m && w && strcmp(s,"\1\2")==0 )
          { mass=fabs(code22_0[k1*NC+k2]->interface->va[m]);
            width=code22_0[k1*NC+k2]->interface->va[w];
            if(mass<MassCutOut && mass+8*width > pmass[0]+pmass[1]
                            && mass+8*width > smin)
            { if((pmass[0]!=M1 || pmass[1]!=M2)&&(pmass[0]!=M2 || pmass[1]!=M1))
              { double ms=pmass[0]+pmass[1];
                double md=pmass[0]-pmass[1];
                double Pcm=sqrt((mass-ms)*(mass+ms)*(mass-md)*(mass+md))/(2*mass);
                mass=sqrt(M1*M1+Pcm*Pcm)+sqrt(M2*M2+Pcm*Pcm);
              }
              vgrid1=makeVGrid(mass,width);
              if(isPole) vgrid=crossVGrids(&vgrid,&vgrid1); else vgrid=vgrid1;
              isPole++;
            }
          }
                   
          if(isPole==0)
          {  vgrid.n=1;
             vgrid.v[0]=v_min;
             vgrid.v[1]=v_max;
             vgrid.pow[0]=5;
          }
//for(i=0;i<grid.n;i++) printf(" (%E %E) ",grid.ul[i],grid.ur[i]); printf("\n");
/*          if(grid.n==1 && pmass[0]+pmass[1]> 1.1*(smin))
                a=f[0]*sigma(x[0])+f[1]*sigma(x[1]);
          else
*/          
           for(i=0;i<vgrid.n;i++)
          {  
             double da;
             if(Fast>0)   da=gauss(s_integrand,vgrid.v[i],vgrid.v[i+1],vgrid.pow[i]);
             else         { int err; da=simpson(s_integrand,vgrid.v[i],vgrid.v[i+1],eps,&err);
                            if(err) {do_err=do_err||err; printf("error in simpson omega.c line 1066\n");}
                          }   
             a+=da;             
          }
      } 

      if(neg_cs_flag && *(CI->gswidth)==0)
      { *(CI->gswidth)=1;
         goto  repeat;
      }   

// printf("X=%.2E (%d) %.3E %s %s %s %s %E %E %E %E\n",X,average, a, pname[0],pname[1],pname[2],pname[3], pMass(pname[0]),M1,pMass(pname[1]),M2);


      for(kk=2;kk<4;kk++) if(pmass[kk]>2*Mcdm && pname[kk][0]!='~')
      {  txtList LL;
         double BrSm=1;
         pWidth(pname[kk],&LL);
         for(;LL;LL=LL->next)
         { double b;
           char proc[40];
           sscanf(LL->txt,"%lf %[^\n]",&b,proc);
           if( strchr(proc,'~'))BrSm-=b;
         }
         a*=BrSm;
      }
      
      if(pname[2][0]=='~' || pname[3][0]=='~' ) { a/=2; Sum1kk+=a;}  
      Sumkk+=a;
      if(wPrc) (*wPrc)[nPrc].weight = a*factor;
    }
    Sum+=factor*Sumkk;
    Sum1+=factor*Sum1kk;
    
/*
printf("Sum=%E\n",Sum);
*/
  }
   
  if(wPrc) 
  { 
//    for(i=0; i<nPrc;i++) printf("wPrc  %s %s -> %s %s %E\n",(*wPrc)[i].prtcl[0],(*wPrc)[i].prtcl[1],(*wPrc)[i].prtcl[2],
//    (*wPrc)[i].prtcl[3],(*wPrc)[i].weight);

    for(i=0; i<nPrc;i++)  (*wPrc)[i].weight/=Sum;    
    for(i=0;i<nPrc-1;)
    {  if((*wPrc)[i].weight >= (*wPrc)[i+1].weight) i++; 
       else
       {  aChannel buff;
          buff=(*wPrc)[i+1];(*wPrc)[i+1]=(*wPrc)[i];(*wPrc)[i]=buff;
          if(i)i--;else i++;
       }
    }          
    if(NPrc) *NPrc=nPrc; 
//    if(nPrc==0) *wPrc=(aChannel*)realloc(*wPrc,sizeof(aChannel));
       
    if(nPrc){ (*wPrc)[nPrc].weight=0; for(i=0;i<5;i++) (*wPrc)[nPrc].prtcl[i]=NULL;} 
  }  
  if(!average) { double gf=geffDM(Mcdm/X);  Sum/=gf*gf; Sum1/=gf*gf;   }
/*
exit(1);
*/
  WIDTH_FOR_OMEGA=0;
  if(alpha) {  *alpha=Sum1/Sum;  /*  printf("ALPHA=%E\n",*alpha);*/    }   
  return Sum;
}



double vSigma(double T,double Beps ,int Fast)
{
    double X=Mcdm/T;
    double res;
    if(assignVal("Q",2*Mcdm+T)==0) calcMainFunc();
    GGscale=(2*Mcdm+T)/3; 
    MassCut=Mcdm*(2-log(Beps)/X);       
    res= 3.8937966E8*aRate(X, 0 ,Fast,NULL,&vSigmaTCh,NULL);
    return res;
}

/*
double Yeq(double T)
{  double heff;
   double X=Mcdm/T;
   heff=hEff(T);  
   return (45/(4*M_PI*M_PI*M_PI*M_PI))*X*X*geffDM(T)*sqrt(M_PI/(2*X))*exp(-X)/heff;
//   double res= (45/(4*M_PI*M_PI*M_PI*M_PI))*X*X*geffDM(T)*sqrt(M_PI/(2*X))*exp(-X)/heff;

// printf("Yeq(%E) X=%E heff=%E MassCut=%E  geffDM=%E res=%e \n",T,X, heff, MassCut, geffDM(T),res);   

//   return res;
}
*/

///*
double Yeq(double T)
{  double heff,s;
   s=2*M_PI*M_PI*T*T*T*hEff(T)/45;
   return  pow(Mcdm*T/(2*M_PI*M_PI),1.5)/s*exp(-Mcdm/T);
}
//*/                        

struct {double*data; double *alpha; double xtop; int pow,size;} vSigmaGrid={NULL,NULL,0,0,0}; 

static void checkSgridUpdate(void)
{
  if(vSigmaGrid.pow==vSigmaGrid.size)
  { vSigmaGrid.size+=20;
    vSigmaGrid.data=(double*)realloc(vSigmaGrid.data,sizeof(double)*vSigmaGrid.size);
    vSigmaGrid.alpha=(double*)realloc(vSigmaGrid.alpha,sizeof(double)*vSigmaGrid.size);
  }       
}

static double vSigmaI(double T, double Beps, int fast,double * alpha_)
{ double XX,alpha;
  int i,n;
  double X=Mcdm/T;
  if(vSigmaGrid.pow==0)
  { checkSgridUpdate();
    vSigmaGrid.pow=1;
    vSigmaGrid.xtop=X;
    MassCut=Mcdm*(2-log(Beps)/X);
    vSigmaGrid.data[0]= aRate(X,0,fast,&alpha,NULL,NULL)+ZeroCS;
    vSigmaGrid.alpha[0]=alpha;
    if(alpha_) *alpha_=alpha;     
    return vSigmaGrid.data[0];
  }
  
  while(X<vSigmaGrid.xtop*XSTEP)
  { XX=vSigmaGrid.xtop/XSTEP;
    checkSgridUpdate();
    for(i=vSigmaGrid.pow;i;i--)
    { vSigmaGrid.data[i]=vSigmaGrid.data[i-1];
      vSigmaGrid.alpha[i]=vSigmaGrid.alpha[i-1];
    }
    vSigmaGrid.xtop=XX;
    MassCut=Mcdm*(2-log(Beps)/XX);
    vSigmaGrid.data[0]=aRate(XX,0,fast,&alpha,NULL,NULL)+ZeroCS;
    vSigmaGrid.alpha[0]=alpha; 
    vSigmaGrid.pow++;
  }
  
  n=log(X/vSigmaGrid.xtop)/log(XSTEP); 

  while(n+2>vSigmaGrid.pow-1)
  { 
    XX=vSigmaGrid.xtop* pow(XSTEP,vSigmaGrid.pow)  ;
    checkSgridUpdate();
    MassCut=Mcdm*(2-log(Beps)/XX);
    vSigmaGrid.data[vSigmaGrid.pow]=aRate(XX,0,fast,&alpha,NULL,NULL)+ZeroCS;
    vSigmaGrid.alpha[vSigmaGrid.pow]=alpha;
    vSigmaGrid.pow++;
  }

  { double X0,X1,X2,X3,sigmav0,sigmav1,sigmav2,sigmav3,alpha0,alpha1,alpha2,alpha3;
    i=log(X/vSigmaGrid.xtop)/log(XSTEP);
    if(i<0)i=0; 
    if(i>vSigmaGrid.pow-2) i=vSigmaGrid.pow-2;
    X0=vSigmaGrid.xtop*pow(XSTEP,n-1); X1=X0*XSTEP;  X2=X1*XSTEP; X3=X2*XSTEP; 

    sigmav0=log(vSigmaGrid.data[n-1]); alpha0=vSigmaGrid.alpha[n-1]; 
    sigmav1=log(vSigmaGrid.data[n]);   alpha1=vSigmaGrid.alpha[n];
    sigmav2=log(vSigmaGrid.data[n+1]); alpha2=vSigmaGrid.alpha[n+1];
    sigmav3=log(vSigmaGrid.data[n+2]); alpha3=vSigmaGrid.alpha[n+2];
    X=log(X);X0=log(X0); X1=log(X1); X2=log(X2); X3=log(X3);
    
    
    if(alpha_)
    { if(alpha1==0) *alpha_=0; 
      else 
      { *alpha_=  alpha0*       (X-X1)*(X-X2)*(X-X3)/        (X0-X1)/(X0-X2)/(X0-X3)
                 +alpha1*(X-X0)*       (X-X2)*(X-X3)/(X1-X0)/        (X1-X2)/(X1-X3)
                 +alpha2*(X-X0)*(X-X1)*       (X-X3)/(X2-X0)/(X2-X1)/        (X2-X3)
                 +alpha3*(X-X0)*(X-X1)*(X-X2)       /(X3-X0)/(X3-X1)/(X3-X2)        ;
        if(*alpha_ <0) *alpha_=0;
      }                  
    }
    double  res=exp( 
    sigmav0*       (X-X1)*(X-X2)*(X-X3)/        (X0-X1)/(X0-X2)/(X0-X3)
   +sigmav1*(X-X0)*       (X-X2)*(X-X3)/(X1-X0)/        (X1-X2)/(X1-X3) 
   +sigmav2*(X-X0)*(X-X1)*       (X-X3)/(X2-X0)/(X2-X1)/        (X2-X3) 
   +sigmav3*(X-X0)*(X-X1)*(X-X2)       /(X3-X0)/(X3-X1)/(X3-X2)        
                    ) -ZeroCS;
    if(res<0) res=0;
    return res;                             
  }
}


static double dY(double s3, double Beps,double fast)  
{ double d, dlnYds3,Yeq0X, sqrt_gStar, vSig,res;;
  double epsY,alpha;
  double T,heff,geff;
  T=T_s3(s3);   
  heff=hEff(T);
  geff=gEff(T);
  MassCut=2*Mcdm-T*log(Beps);
  d=0.001*s3;  dlnYds3=( log(Yeq(T_s3(s3+d)))- log(Yeq(T_s3(s3-d))) )/(2*d);  // ???

  epsY=deltaY/Yeq(T);

//  sqrt_gStar=polint1(Mcdm/X,Tdim,t_,sqrt_gstar_);

  vSig=vSigmaI(T,Beps, fast,&alpha);
  if(vSig <=0) return 10;
  if(vSig==0){ FError=1; return 0;}
  res= dlnYds3/(pow(2*M_PI*M_PI/45.*heff,0.66666666)/sqrt(8*M_PI/3.*M_PI*M_PI/30.*geff)*vSig*MPlank
  *(1-alpha/2)*sqrt(1+epsY*epsY))/Yeq(T);
  res=fabs(res);
  if(res>10) return 10;
  return res;
} 


static double darkOmega1(double * Xf,double Z1,double dZ1,int Fast,double Beps)
{
  double X = *Xf;
  double CCX=(Z1-1)*(Z1+1);
  double dCCX=(Z1-1+dZ1)*(Z1+1+dZ1)-CCX;
  double ddY;
  double dCC1,dCC2,X1,X2;
    
  gaussInt= Fast? 1 : 0;

  if(Beps>=1) Beps=0.999;
  vSigmaGrid.pow=0;
  
  ddY=dY(s3_T(Mcdm/X) ,Beps,Fast); 
  if(FError || ddY==0)  return -1;
  if(fabs(CCX-ddY)<dCCX) 
  { *Xf=X; MassCut=Mcdm*(2-log(Beps)/X); 
    return Yeq(Mcdm/X)*sqrt(1+ddY);
  } 
   
  dCC1=dCC2=ddY-CCX; ;X1=X2=X; 
  while(dCC2>0) 
  {  
     X1=X2;
     dCC1=dCC2;
     X2=X2/XSTEP;
     X=X2;
     dCC2=-CCX+dY(s3_T(Mcdm/X),Beps,Fast);
     if(X<2)  return -1;   
     if(Mcdm/X>1.E5) return -1;
  }
             
  while (dCC1<0)
  {  
     X2=X1;
     dCC2=dCC1;
     X1=X1*XSTEP;
     X=X1;
     dCC1=-CCX+dY(s3_T(Mcdm/X),Beps,Fast); 
  }
  for(;;)
  { double dCC;
    if(fabs(dCC1)<dCCX) 
      {*Xf=X1; MassCut=Mcdm*(2-log(Beps)/X1); return Yeq(Mcdm/X1)*sqrt(1+CCX+dCC1);}
    if(fabs(dCC2)<dCCX || fabs(X1-X2)<0.0001*X1) 
      {*Xf=X2; MassCut=Mcdm*(2-log(Beps)/X2); return Yeq(Mcdm/X2)*sqrt(1+CCX+dCC2);}
    X=0.5*(X1+X2); 
    dCC=-CCX+dY(s3_T(Mcdm/X),Beps,Fast);
    if(dCC>0) {dCC1=dCC;X1=X;}  else {dCC2=dCC;X2=X;} 
  }
}

static double Beps_;
int Fast_=1;

static void XderivLn(double s3, double *Y, double *dYdx)
{
  double y=Y[0];
  double yeq, sqrt_gStar;
  double T,heff,geff;
  
//  s3=polint1(T,Tdim,t_,s3_);  
  T=T_s3(s3);  
  heff=hEff(T);
  geff=gEff(T);
//  sqrt_gStar=polint1(T,Tdim,t_,sqrt_gstar_);
  
  MassCut=2*Mcdm -T*log(Beps_); yeq=Yeq(T);
//  if(y<yeq) *dYdx=0; else 
  { double vSig,alpha,epsY;
  
    if(deltaY) epsY=deltaY/y; else  epsY=0; 
    vSig=vSigmaI(T,Beps_,Fast_,&alpha);
//printf("T=%E alpha=%E\n", Mcdm/x, alpha);     
    *dYdx=MPlank
    *pow(2*M_PI*M_PI/45.*heff,0.666666666666)/sqrt(8*M_PI/3.*M_PI*M_PI/30.*geff)
//    *sqrt_gStar*sqrt(M_PI/45)
    *vSig*(y*y-(1-alpha)*yeq*yeq-alpha*y*yeq)*sqrt(1+epsY*epsY);
//printf(" T=%E  y=%E   yeq=%E  epsY=%E  alpha=%E \n",T, y,  yeq, epsY, alpha);   
  }
}


double darkOmegaFO(double * Xf_, int Fast, double Beps)
{
  double Yf;
  double Z1=2.5;
  double dZ1=0.05;
  double Xf=25;

  if(CDM1==NULL) fracCDM2=1; else
  if(CDM2==NULL) fracCDM2=0; else 
  if(Mcdm1<Mcdm2) fracCDM2=0; else fracCDM2=1;
  
  if(omegaCh) {free(omegaCh); omegaCh=NULL;}
    
  if(Xf_) *Xf_=Xf; 
  if(assignVal("Q",2*Mcdm)==0) calcMainFunc();
  GGscale=2*Mcdm/3;   
  if(Beps>=1) Beps=0.999;
  
  Yf=  darkOmega1(&Xf, Z1, dZ1,Fast, Beps);
  if(FError||Xf<1||Yf<=0) {  return -1;}
  
  double iColl=( (Mcdm/Xf)*sqrt(M_PI/45)*MPlank*aRate(Xf, 1,Fast,NULL, NULL,NULL) );

  if(Xf_) *Xf_=Xf; 
  
  if(FError) return -1;
  return  2.742E8*Mcdm/(1/Yf +  iColl); /* 2.828-old 2.755-new 2.742 next-new */
}



static double *Ytab=NULL;

double YF(double T){ return polint3(T,Ntab,Ttab, Ytab) ;}


double darkOmega(double * Xf, int Fast, double Beps,int *err)
{
  double Yt,Xt=27;
  double Z1=1.1,Z2=10,Zf=2.5; 
  int i;
  int Nt=25;
  if(err) *err=0;

  double Mcdm_mem=Mcdm;
  Mcdm=-1;
  for(i=0;i<NC;i++) if(!feebleParticles[oddPpos[i]] && (Mcdm<0 ||  Mcdm > inMass[i])) Mcdm=inMass[i]; 
  
  if(Mcdm<0) { printf(" There are no  Dark Matter particles\n"); Mcdm=Mcdm_mem; return 0;} 
  
  Ytab=realloc(Ytab,sizeof(double)*Nt);
  Ttab=realloc(Ttab,sizeof(double)*Nt);
  Ntab=0;

  if(CDM1==NULL)  fracCDM2=1; else
  if(CDM2==NULL)  fracCDM2=0; else 
  if(Mcdm1<Mcdm2) fracCDM2=0; else fracCDM2=1;

  if(assignVal("Q",2*Mcdm)==0) calcMainFunc() ;
  GGscale=2*Mcdm/3;
  if(Beps>=1) Beps=0.999;
  Beps_=Beps; Fast_=Fast;
  
  if(Z1<=1) Z1=1.1;
  
  Yt=  darkOmega1(&Xt, Z1, (Z1-1)/5,Fast, Beps);

  if(Yt<0||FError) 
  {  Mcdm=Mcdm_mem; 
     if(err) *err=1; else printf("Temperature of thermal  equilibrium is too large\n");   
     return -1;
  }
  
  Tstart=Mcdm/Xt;
  
  if(Yt<fabs(deltaY)*1.E-15)
  {  
     if(deltaY>0) dmAsymm=1;  else dmAsymm=-1;
     if(Xf) *Xf=Xt;   
     return 2.742E8*Mcdm*deltaY;  
  }   
  
  Ntab=1;
  Ttab[0]=Tstart;
  Ytab[0]=Yt;
  Tend=Tstart;
  
  for(i=0; ;i++)
  { double X2=vSigmaGrid.xtop*pow(XSTEP,i+1);
    double yeq,alpha;
    double s3_t,s3_2;

    if(Xt>X2*0.999999) continue; 

    yeq=Yeq(Mcdm/Xt);
    alpha=vSigmaGrid.alpha[i];    


    if(Yt*Yt>=Z2*Z2*( alpha*Yt*yeq+(1-alpha)*yeq*yeq) || Yt<fabs(deltaY*1E-15))  break;
    

    s3_t=s3_T(Mcdm/Xt);
    s3_2=s3_T(Mcdm/X2); 
//    if(odeint(&y,1 ,Mcdm/Xt , Mcdm/X2 , 1.E-3, (Mcdm/Xt-Mcdm/X2 )/2, &XderivLn)){ printf("problem in solving diff.equation\n"); return -1;}   
    if(odeint(&Yt,1 ,s3_t , s3_2 , 1.E-3, (s3_2-s3_t)/2, &XderivLn)){ printf("problem in solving diff.equation\n"); return -1;}
    if(Ntab>=Nt)
    { Nt+=20;
      Ytab=realloc(Ytab,sizeof(double)*Nt);
      Ttab=realloc(Ttab,sizeof(double)*Nt);
    }      
    
    Tend=Mcdm/X2;                                 
    Xt=X2;   
    Ytab[Ntab]=Yt;
    Ttab[Ntab]=Tend;
    Ntab++;
  }
  
  if(Xf) 
  {  double T1,T2,Y1,Y2,dY2,dY1;
     T1=Ttab[0];
     Y1=Ytab[0];
     dY1=Zf*Yeq(T1)-Y1;
     *Xf=Mcdm/T1;
     for(i=1;i<Ntab;i++)            
     { T2=Ttab[i];
       Y2=Ytab[i]; 
       dY2=Zf*Yeq(T2)-Y2;
       if(dY2<0)
       { 
         for(;;)
         {  double al,Tx,Yx,dYx,Xx;
            al=dY2/(dY2-dY1);
            Tx=al*T1+(1-al)*T2, /*Yx=al*Y1+(1-al)*Y2,*/ Yx=polint3(Tx,Ntab,Ttab,Ytab),    dYx=Zf*Yeq(Tx)-Yx;
            if(fabs(dYx)<0.01*Yx) 
            { *Xf=Mcdm/Tx;
              break;
            } else  { if(dYx>0) {T1=Tx,Y1=Yx;}  else {T2=Tx,Y2=Yx;} }
         }
         break; 
      }  
      else {dY1=dY2; T1=T2; Y1=Y2; *Xf=Mcdm/T2;}        
    }
  }
     
  if(Yt<fabs(deltaY*1E-15))  
  {  
      if(deltaY>0) dmAsymm=1; else dmAsymm=-1;
      Mcdm=Mcdm_mem;   
      return 2.742E8*Mcdm*deltaY;
  }  


  double iColl=( (Mcdm/Xt)*sqrt(M_PI/45)*MPlank*aRate(Xt,1,Fast,NULL,NULL,NULL));
  Mcdm=Mcdm_mem;
  if(FError) { if(err) *err=8;  return -1;}
  
  if(deltaY==0)
  { dmAsymm=0;
    return  2.742E8*Mcdm/(1/Yt  + iColl); /* 2.828-old 2.755-new,2.742 -newnew */
  } else
  {  double a,f,z0,Y0;
     a=fabs(deltaY);
     if(Yt<a*1.E-5)  f=Yt*Yt/4/a; else f=(sqrt(Yt*Yt+a*a)-a)/(sqrt(Yt*Yt+a*a)+a);   
     f*= exp(-2*a*iColl);
     z0=sqrt(f)*2*a/(1-f);
     Y0=sqrt(z0*z0+a*a);
     dmAsymm=deltaY/Y0;
     return 2.742E8*Mcdm*Y0;
  }    
}


double printChannels(double Xf ,double cut, double Beps, int prcn, FILE * f)
{ int i,nPrc,nform=log10(1/cut)-2;
  double Sum,s;

  double Mcdm_mem=Mcdm;
  Mcdm=-1;
  for(i=0;i<NC;i++) if(!feebleParticles[oddPpos[i]] && (Mcdm<0 ||  Mcdm > inMass[i])) Mcdm=inMass[i]; 
  
  if(omegaCh) {free(omegaCh); omegaCh=NULL;}

  MassCut=Mcdm*(2-log(Beps)/Xf);
  Sum=aRate(Xf, 1,1,NULL,&omegaCh,&nPrc)*(Mcdm/Xf)*sqrt(M_PI/45)*MPlank/(2.742E8*Mcdm_mem); 
  if(Sum==0 || FError)     { return -1;}
  if(nform<0)nform=0;
   
  if(f)
  {  int j;
     fprintf(f,"# Channels which contribute to 1/(omega) more than %G%%.\n",100*cut );
     if(prcn) fprintf(f,"# Relative contributions in %% are displayed\n");
        else  fprintf(f,"# Absolute contributions are displayed\n");
     for(i=0,s=0;i<nPrc;i++)  if(fabs(omegaCh[i].weight)>=cut)
     {  s+=omegaCh[i].weight;
        if(prcn)
        { if(cut <0.000001) fprintf(f,"  %.1E%% ",100*omegaCh[i].weight);
          else              fprintf(f,"  %*.*f%% ",nform+3,nform,
                                        100*omegaCh[i].weight);
        } else fprintf(f,"  %.1E ",Sum*omegaCh[i].weight); 
        for(j=0;j<4;j++)
        {
           fprintf(f,"%s ",omegaCh[i].prtcl[j]);
           if(j==1) fprintf(f,"->");
           if(j==3) fprintf(f,"\n");
        }
     }
  }
  
  Mcdm=Mcdm_mem;
  
  return 1/Sum;
}

static int strcmp_(char * n1, char *n2) { if( n1[0]=='*' &&  n1[1]==0) return 0; return strcmp(n1, n2);}

double oneChannel(double Xf,double Beps,char*n1,char*n2,char*n3,char*n4)
{ int j,nPrc;
  aChannel *wPrc;
  double Sum,res;

  MassCut=Mcdm*(2-log(Beps)/Xf);
  Sum=aRate(Xf, 1,1,NULL,&wPrc,&nPrc)*(Mcdm/Xf)*sqrt(M_PI/45)*MPlank/(2.742E8*Mcdm);
  if(FError)     { return -1;}
  if(wPrc==NULL) { return  0;}  

  for(res=0,j=0;j<nPrc;j++) 
  if( ( (strcmp_(n1,wPrc[j].prtcl[0])==0 && strcmp_(n2,wPrc[j].prtcl[1])==0) ||
        (strcmp_(n2,wPrc[j].prtcl[0])==0 && strcmp_(n1,wPrc[j].prtcl[1])==0)
      ) &&
      ( (strcmp_(n3,wPrc[j].prtcl[2])==0 && strcmp_(n4,wPrc[j].prtcl[3])==0) ||
        (strcmp_(n4,wPrc[j].prtcl[2])==0 && strcmp_(n3,wPrc[j].prtcl[3])==0)
      )
    )  {res+=wPrc[j].weight;} 
      
  free(wPrc); 
  return res;
}
//================= Ext vSigma =======

static double vSigmaZero(double T){return 0;}
static double (*vSigmaStat0)(double T)=vSigmaZero;
static double (*vSigmaStat1)(double T)=vSigmaZero;

//derivative w.r.t entropy
static void XderivLnExt(double s3, double *Y, double *dYdx)
{
  double y=Y[0];
  double yeq, sqrt_gStar;
  double T,heff,geff;
  double vSig,vSig0,vSig1,alpha,epsY;
  
  T=T_s3(s3);  
  yeq=Yeq(T);
  if(y<=yeq) {*dYdx=0; return;} else epsY=deltaY/y;
  
  
  vSig0=vSigmaStat0(T)/3.8937966E8;//GeV−2 = 0.3894 mb = 3.894E8 pb
  vSig1=vSigmaStat1(T)/3.8937966E8;
  vSig=(vSig0+vSig1);
    
  if(vSig==0) {*dYdx=0; return;} else alpha=vSig1/(vSig0+vSig1);
  
  heff=hEff(T);
  geff=gEff(T);

  *dYdx=MPlank
      *pow(2*M_PI*M_PI/45.*heff,0.666666666666)/sqrt(8*M_PI/3.*M_PI*M_PI/30.*geff)
      *vSig*(y*y-(1-alpha)*yeq*yeq-alpha*y*yeq)*sqrt(1+epsY*epsY); 
}


//derivative w.r.t entropy
static double dYExt(double s3)  
{ double d, dlnYds3,Yeq0X, sqrt_gStar, vSig,vSig0,vSig1,res;;
  double epsY,alpha,yeq;
  double T,heff,geff;
  T=T_s3(s3);   
  yeq=Yeq(T);
  if(yeq<=0) return 10;
  epsY=deltaY/yeq;
  vSig0=vSigmaStat0(T)/3.8937966E8;
  vSig1=vSigmaStat1(T)/3.8937966E8;
  vSig=vSig0+vSig1;
  if(vSig <=0) return 10; 
  alpha=vSig1/vSig;
  
  
  heff=hEff(T);
  geff=gEff(T);
  d=0.001*s3;  dlnYds3=( log(Yeq(T_s3(s3+d)))
                        -log(Yeq(T_s3(s3-d))) )/(2*d);

  res= dlnYds3/(pow(2*M_PI*M_PI/45.*heff,0.66666666)/sqrt(8*M_PI/3.*M_PI*M_PI/30.*geff)
      *vSig*MPlank*(1-alpha/2)*sqrt(1+epsY*epsY))/Yeq(T);
  res=fabs(res);
  if(res>10) return 10;
  return res;
} 


//derivative w.r.t entropy
static double darkOmega1Ext(double * Xf,double Z1,double dZ1)
{
  double X = *Xf;
  double CCX=(Z1-1)*(Z1+1);
  double dCCX=(Z1-1+dZ1)*(Z1+1+dZ1)-CCX;
  double ddY;
  double dCC1,dCC2,X1,X2;

  
  ddY=dYExt(s3_T(Mcdm/X)); 
  if(FError || ddY==0)  return -1;
  if(fabs(CCX-ddY)<dCCX) 
  { *Xf=X;
    return Yeq(Mcdm/X)*sqrt(1+ddY);
  } 
   
  dCC1=dCC2=ddY-CCX; ;X1=X2=X; 
  while(dCC2>0) 
  {  
     X1=X2;
     dCC1=dCC2;
     X2=X2/XSTEP;
     X=X2;
     dCC2=-CCX+dYExt(s3_T(Mcdm/X));
     if(Mcdm/X>1.E5) return -1;
  }
             
  while (dCC1<0)
  {  
     X2=X1;
     dCC2=dCC1;
     X1=X1*XSTEP;
     X=X1;
     dCC1=-CCX+dYExt(s3_T(Mcdm/X)); 
  }
  for(;;)
  { double dCC;
    if(fabs(dCC1)<dCCX) 
      {*Xf=X1;  return Yeq(Mcdm/X1)*sqrt(1+CCX+dCC1);}
    if(fabs(dCC2)<dCCX || fabs(X1-X2)<0.0001*X1) 
      {*Xf=X2;  return Yeq(Mcdm/X2)*sqrt(1+CCX+dCC2);}
    X=0.5*(X1+X2); 
    dCC=-CCX+dYExt(s3_T(Mcdm/X));
    if(dCC>0) {dCC1=dCC;X1=X;}  else {dCC2=dCC;X2=X;} 
  }
}



//derivative w.r.t entropy
double darkOmegaExt(double * Xf,  double (*f0)(double),double (*f1)(double))
{
  double Yt,Xt=15;
  double Z1=1.1;
  double Z2=10,Zf=2.5; 
  int i;
  
  if(f0) vSigmaStat0=f0; else vSigmaStat0=vSigmaZero;
  if(f1) vSigmaStat1=f1; else vSigmaStat1=vSigmaZero;
  
  MassCut=4*Mcdm;

  int Nt=25;
  
  Ytab=realloc(Ytab,sizeof(double)*Nt);
  Ttab=realloc(Ttab,sizeof(double)*Nt);
  Ntab=0;

  if(CDM1==NULL) fracCDM2=1; else
  if(CDM2==NULL) fracCDM2=0; else 
  if(Mcdm1<Mcdm2)fracCDM2=0; else fracCDM2=1;

  Yt=  darkOmega1Ext(&Xt, Z1, (Z1-1)/5);
  printf("initial x: %3e \n",Xt);
  //Yt = Yeq(Mcdm/Xt)*1.1;

  if(Yt<0||FError) { return -1;}
  
  if(Yt<fabs(deltaY)*1.E-15)
  {  
     if(deltaY>0) dmAsymm=1;  else dmAsymm=-1;
     if(Xf) *Xf=Xt;   
     return 2.742E8*Mcdm*deltaY;
  }     
  
  Tstart=Mcdm/Xt;
  Ntab=1;
  Ttab[0]=Tstart;
  Ytab[0]=Yt;
  Tend=Tstart;
         
  for(i=0; ;i++)
  { 
    double s3_t,s3_2,Tbeg;
    double yeq=Yeq(Mcdm/Xt);

    if(Tend<1.E-3 || Yt<fabs(deltaY*1E-5)) break;
    Tbeg=Tend;       
    Tend/=1.2;                                 
    s3_t=s3_T(Tbeg);
    s3_2=s3_T(Tend); 
    if(odeint(&Yt,1 ,s3_t , s3_2 , 1.E-3, (s3_2-s3_t)/2, &XderivLnExt)){ printf("problem in solving diff.equation\n"); return -1;}
    if(!isfinite(Yt)||FError)  return -1;
    if(Ntab>=Nt)
    { Nt+=20;   
      Ytab=realloc(Ytab,sizeof(double)*Nt);
      Ttab=realloc(Ttab,sizeof(double)*Nt);
    }      
    Ytab[Ntab]=Yt;
    Ttab[Ntab]=Tend;
    Ntab++;  
    Tbeg=Tend;                                           
  }  

  if(Xf) 
  {  double T1,T2,Y1,Y2,dY2,dY1;
     T1=Ttab[0];
     Y1=Ytab[0];
     dY1=Zf*Yeq(T1)-Y1;  
     *Xf=Mcdm/T1;
     for(i=1;i<Ntab;i++)            
     { T2=Ttab[i];
       Y2=Ytab[i]; 
       dY2=Zf*Yeq(T2)-Y2;
       if(dY2<0)
       {          
         for(;;)
         {  double al,Tx,Yx,dYx,Xx;
            al=dY2/(dY2-dY1);
            Tx=al*T1+(1-al)*T2, Yx=polint3(Tx,Ntab,Ttab,Ytab),   dYx=Zf*Yeq(Tx)-Yx;
            if(fabs(dYx)<0.01*Yx) 
            { *Xf=Mcdm/Tx;
              break;
            } else  { if(dYx>0) {T1=Tx,Y1=Yx;}  else {T2=Tx,Y2=Yx;} }
         }
         break; 
      }  
      else {dY1=dY2; T1=T2; Y1=Y2; *Xf=Mcdm/T2;}        
    }
  }

  if(Yt<fabs(deltaY*1E-15))  
  {  
      if(deltaY>0) dmAsymm=1; else dmAsymm=-1;   
      return 2.742E8*Mcdm*deltaY;
  }  
//  Yi=1/( (Mcdm/Xt)*sqrt(M_PI/45)*MPlank*aRate(Xt,1,Fast,NULL,NULL,NULL));
  if(deltaY==0)
  { dmAsymm=0;
    return  2.742E8*Mcdm*Yt; 
    // 2.828-old 2.755-new,2.742 -newnew 
  } else
  {  double a,f,z0,Y0;
     if(Yt<fabs(deltaY*1E-15))
     { if(deltaY>0) dmAsymm=1; else dmAsymm=-1;
       return 2.742E8*Mcdm*deltaY;
     }
     a=fabs(deltaY);
     if(Yt<a*1.E-5)  f=Yt*Yt/4/a; else f=(sqrt(Yt*Yt+a*a)-a)/(sqrt(Yt*Yt+a*a)+a);   
     z0=sqrt(f)*2*a/(1-f);
     Y0=sqrt(z0*z0+a*a);
     dmAsymm=deltaY/Y0;
     return 2.742E8*Mcdm*Y0;
  }   
}

/* ========== derivative w.r.t xf =====================
//  
// ================================================== */

//derivative w.r.t xf
static void XderivLnExt1(double x, double *Y, double *dYdx)
{
  double y=Y[0];
  double yeq, sqrt_gStar;
  double T,heff,geff;
  double vSig,vSig0,vSig1,alpha,epsY;
  
  //T=T_s3(s3);  
  //yeq=Yeq(T);
  yeq=Yeq(Mcdm/x);
  if(y<=yeq) {*dYdx=0; return;} else epsY=deltaY/y;
  
  
  vSig0=vSigmaStat0(Mcdm/x)/3.8937966E8;//GeV−2 = 0.3894 mb = 3.894E8 pb
  vSig1=vSigmaStat1(Mcdm/x)/3.8937966E8;
  vSig=(vSig0+vSig1);
    
  if(vSig==0) {*dYdx=0; return;} else alpha=vSig1/(vSig0+vSig1);
  
  //heff=hEff(T);
  //geff=gEff(T);
  *dYdx=-(Mcdm/x/x)*MPlank*sqrt(gEff(Mcdm/x))*sqrt(M_PI/45)*vSig*(y*y-yeq*yeq);
  //printf("y difference: %4e\n",(y-yeq));
}

//derivative w.r.t xf
static double dYExt1(double x)  
{ double d, dYdX,Yeq0X, sqrt_gStar, vSig,vSig0,vSig1,res;;
  double epsY,alpha,yeq;
  double T,heff,geff;
  double cFactor=sqrt(M_PI/45)*MPlank*Mcdm;
  //T=T_s3(s3);   
  yeq=Yeq(Mcdm/x);
  if(yeq<=0) return 10;
  
  vSig0=vSigmaStat0(Mcdm/x)/3.8937966E8;
  vSig1=vSigmaStat1(Mcdm/x)/3.8937966E8;
  vSig=vSig0+vSig1;
  if(vSig <=0) return 10; 
  alpha=vSig1/vSig;
  
  d=x*0.01; dYdX=(Yeq(Mcdm/(x+d))-Yeq(Mcdm/(x-d)))/(2*d);
  Yeq0X=Yeq(Mcdm/x)/x;

  res= dYdX/(vSig*cFactor*sqrt(gEff(Mcdm/x))*Yeq0X*Yeq0X);
  res=fabs(res);
  if(res>10) return 10;
  return res;
} 

//derivative w.r.t xf
static double darkOmega1Ext1(double * Xf,double Z1,double dZ1)
{
  double X = *Xf;
  double CCX=(Z1-1)*(Z1+1);
  double dCCX=(Z1-1+dZ1)*(Z1+1+dZ1)-CCX;
  double ddY;
  double dCC1,dCC2,X1,X2;
  double cFactor=sqrt(M_PI/45)*MPlank*Mcdm;

  
  ddY=dYExt(s3_T(Mcdm/X)); 
  if(FError || ddY==0)  return -1;
  if(fabs(CCX-ddY)<dCCX) 
  { *Xf=X;
    return Yeq(Mcdm/X)*sqrt(1+ddY);
  } 
   
  dCC1=dCC2=ddY-CCX; ;X1=X2=X; 
  while(dCC2>0) 
  {  
     X1=X2;
     dCC1=dCC2;
     X2=X2/XSTEP;
     X=X2;
     dCC2=-CCX+dYExt1(X);
     if(Mcdm/X>1.E5) return -1;
  }
             
  while (dCC1<0)
  {  
     X2=X1;
     dCC2=dCC1;
     X1=X1*XSTEP;
     X=X1;
     dCC1=-CCX+dYExt1(X); 
  }
  for(;;)
  { double dCC;
    if(fabs(dCC1)<dCCX) 
      {*Xf=X1;  return Yeq(Mcdm/X1)*sqrt(1+CCX+dCC1);}
    if(fabs(dCC2)<dCCX || fabs(X1-X2)<0.0001*X1) 
      {*Xf=X2;  return Yeq(Mcdm/X2)*sqrt(1+CCX+dCC2);}
    X=0.5*(X1+X2); 
    dCC=-CCX+dYExt1(X);
    if(dCC>0) {dCC1=dCC;X1=X;}  else {dCC2=dCC;X2=X;} 
  }
}

double darkOmegaExt1(double * Xf,  double (*f0)(double),double (*f1)(double))
{
  double Yt,Xt=15;
  double Z1=1.1;
  double Z2=10,Zf=2.5; 
  double y;
  int i;
  
  if(f0) vSigmaStat0=f0; else vSigmaStat0=vSigmaZero;
  if(f1) vSigmaStat1=f1; else vSigmaStat1=vSigmaZero;
  
  MassCut=4*Mcdm;

  int Nt=25;
  
  Ytab=realloc(Ytab,sizeof(double)*Nt);
  Ttab=realloc(Ttab,sizeof(double)*Nt);
  Ntab=0;

  if(CDM1==NULL) fracCDM2=1; else
  if(CDM2==NULL) fracCDM2=0; else 
  if(Mcdm1<Mcdm2)fracCDM2=0; else fracCDM2=1;

  //Yt=  darkOmega1Ext1(&Xt, Z1, (Z1-1)/5);
  Yt = Yeq(Mcdm/Xt)*1.1;
  printf("initial x: %3e \n",Xt);


  if(Yt<0||FError) { return -1;}
     
  
  Tstart=Mcdm/Xt;
  Ntab=1;
  Ttab[0]=Tstart;
  Ytab[0]=Yt;
  Tend=Tstart;
         
  for(i=0; ;i++)
  { 
    double X2,Tbeg;
    double yeq=Yeq(Mcdm/Xt);
    printf("X:%3e,\t ydiff: %4e\t temp:%3e\n",Xt,Yt/(Yeq(Mcdm/Xt)),Mcdm/Xt);

    if(Tend<1.E-3 || Yt<fabs(deltaY*1E-5)) break;
    Tbeg=Tend;       
    Tend/=1.2;                                 
    Xt=Mcdm/(Tbeg);
    X2=Mcdm/(Tend); 
    //y=log(Yt);
    odeint(&Yt,1 , Xt , X2 , 1.E-3, (X2-Xt)/2, &XderivLnExt1); 
    //Yt=exp(y);
    Xt=X2;
    //if(odeint(&Yt,1 ,s3_t , s3_2 , 1.E-3, (s3_2-s3_t)/2, &XderivLnExt)){ printf("problem in solving diff.equation\n"); return -1;}
    if(!isfinite(Yt)||FError)  return -1;
    if(Ntab>=Nt)
    { Nt+=20;   
      Ytab=realloc(Ytab,sizeof(double)*Nt);
      Ttab=realloc(Ttab,sizeof(double)*Nt);
    }      
    Ytab[Ntab]=Yt;
    Ttab[Ntab]=Tend;
    Ntab++;  
    Tbeg=Tend;                                           
  }  

  if(Xf) 
  {  double T1,T2,Y1,Y2,dY2,dY1;
     T1=Ttab[0];
     Y1=Ytab[0];
     dY1=Zf*Yeq(T1)-Y1;  
     *Xf=Mcdm/T1;
     for(i=1;i<Ntab;i++)            
     { T2=Ttab[i];
       Y2=Ytab[i]; 
       dY2=Zf*Yeq(T2)-Y2;
       if(dY2<0)
       {          
         for(;;)
         {  double al,Tx,Yx,dYx,Xx;
            al=dY2/(dY2-dY1);
            Tx=al*T1+(1-al)*T2, Yx=polint3(Tx,Ntab,Ttab,Ytab),   dYx=Zf*Yeq(Tx)-Yx;
            if(fabs(dYx)<0.01*Yx) 
            { *Xf=Mcdm/Tx;
              break;
            } else  { if(dYx>0) {T1=Tx,Y1=Yx;}  else {T2=Tx,Y2=Yx;} }
         }
         break; 
      }  
      else {dY1=dY2; T1=T2; Y1=Y2; *Xf=Mcdm/T2;}        
    }
  }

  if(deltaY==0)  
  {     
      return 2.742E8*Mcdm*Yt;
      //2.742E8*Mcdm*Yt
  }  
  else{
    printf("error in relic density calcultion");
    return -1;
  }
}

/*===================
      Coscattering
=====================*/
///*
static double neq_yintegrand(double y, double mft){
  return y*sqrt(y*y-mft*mft)/(exp(y)+1);
}

static double neq_uintegrand(double u){
  double z, yy;
  Me_ = 5.011E-4;
  if(u==0. || u==1.) return 0.;
  z=1-u*u;
  yy=(Me_/T_)-log(z);
  //yy=(Me_/T_)-T_*log(z);
  return neq_yintegrand(yy, Me_/T_)*2*u/(z);
}

double neqF( double T, double mf, double g)
{  double sv_tot;
   T_=T;
   Me_ = mf;
   //sv_tot=gauss(neq_uintegrand,0.,1.,5);
   //sv_tot=simpson(neq_uintegrand,0.,1.,1E-3,NULL); 
   //return sv_tot*T*T*T*g/2/(M_PI*M_PI);
   ///*
   if(T/mf>=5){
     //sv_tot=simpson(neq_uintegrand,0.,1.,1E-4,NULL); 
     return 0.75*1.202*T*T*T*g/(M_PI*M_PI);
     }
    else if(T/mf>0.5){
     //sv_tot=simpson(neq_uintegrand,0.,1.,1E-4,NULL); 
     sv_tot=gauss345(neq_uintegrand,0.,1.,1E-3,NULL); 
     return sv_tot*T*T*T*g/2/(M_PI*M_PI);
    }
   else{
     return g*pow(mf*T/2/M_PI,1.5)*exp(-mf/T);
   }
   //*/
}


static void XderivLnCosc1(double x, double *Y, double *dYdx)
{
  double y=Y[0];
  double yeq, sqrt_gStar;
  double T,heff,geff;
  double vSig0,coeff;
  
  T=Mcdm/x;  
  //yeq=Yeq(T);
  yeq=Yeq(Mcdm/x);
  //if(y<=yeq) {*dYdx=0; return;} else epsY=deltaY/y;
  
  
  vSig0=vSigmaStat0(Mcdm/x)/3.8937966E8;//GeV−2 = 0.3894 mb = 3.894E8 pb
    
  //if(vSig==0) {*dYdx=0; return;} else alpha=vSig1/(vSig0+vSig1);
  
  //heff=hEff(T);
  //geff=gEff(T);
  //coeff = -(3*1.202/4)*ge*(T/x/sqrt(8*pow(M_PI,7)))*MPlank*sqrt(1/gEff(T))*sqrt(90)*vSig0;
  coeff = -neqF(T,5.11E-4,2)*(1/pow(T,2)/x/sqrt(8*pow(M_PI,3)))*MPlank*sqrt(1/gEff(T))*sqrt(90)*vSig0;
  //printf("cosc factor: %4e \n",coeff);
  //printf("y difference: %4e\n",(yeq/y));
  *dYdx=coeff*(y-yeq);
}



double darkOmegaCosc1(double * Xf,  double (*f0)(double))
{
  double Yt,Xt=7;
  double Z1=1.1;
  double Z2=10,Zf=2.5; 
  double y;
  int i;
  
  if(f0) vSigmaStat0=f0; else vSigmaStat0=vSigmaZero;
  
  MassCut=4*Mcdm;

  int Nt=25;
  
  Ytab=realloc(Ytab,sizeof(double)*Nt);
  Ttab=realloc(Ttab,sizeof(double)*Nt);
  Ntab=0;

  if(CDM1==NULL) fracCDM2=1; else
  if(CDM2==NULL) fracCDM2=0; else 
  if(Mcdm1<Mcdm2)fracCDM2=0; else fracCDM2=1;

  //Yt=  darkOmega1Ext1(&Xt, Z1, (Z1-1)/5);
  Yt = Yeq(Mcdm/Xt)*1.1;
  printf("initial x: %3e \n",Xt);
  printf("initial Yt: %3e \n",Yt);
  

  if(Yt<0||FError) { return -1;}
     
  
  Tstart=Mcdm/Xt;
  Ntab=1;
  Ttab[0]=Tstart;
  Ytab[0]=Yt;
  Tend=Tstart;
         
  for(i=0; ;i++)
  { 
    double X2,Tbeg;
    double yeq=Yeq(Mcdm/Xt);
    //printf("X:%3e,\t ydiff: %4e\t temp:%3e\n",Xt,Yt/(Yeq(Mcdm/Xt)),Mcdm/Xt);

    if(Tend<5.E-4) break;
    //if(Yt>10*yeq) break;
    Tbeg=Tend;       
    Tend/=1.15;                                 
    Xt=Mcdm/(Tbeg);
    X2=Mcdm/(Tend); 
    //y=log(Yt);
    odeint(&Yt,1 , Xt , X2 , 1.E-3, (X2-Xt)/2, &XderivLnCosc1); 
    //Yt=exp(y);
    Xt=X2;
    if(!isfinite(Yt)||FError)  return -1;
    if(Ntab>=Nt)
    { Nt+=20;   
      Ytab=realloc(Ytab,sizeof(double)*Nt);
      Ttab=realloc(Ttab,sizeof(double)*Nt);
    }      
    Ytab[Ntab]=Yt;
    Ttab[Ntab]=Tend;
    Ntab++;  
    Tbeg=Tend; 
    printf("X:%3e,\t y1diff: %4e \n",Xt,Ytab[Ntab-1]/Ytab[Ntab-2]);                                          
  }  
  printf("number of steps: %3i\n",Ntab);

  if(Xf) 
  {  double T1,T2,Y1,Y2,dY2,dY1;
     T1=Ttab[0];
     Y1=Ytab[0];
     dY1=Zf*Yeq(T1)-Y1;  
     *Xf=Mcdm/T1;
     for(i=1;i<Ntab;i++)            
     { T2=Ttab[i];
       Y2=Ytab[i]; 
       dY2=Zf*Yeq(T2)-Y2;
       if(dY2<0)
       {          
         for(;;)
         {  double al,Tx,Yx,dYx,Xx;
            al=dY2/(dY2-dY1);
            Tx=al*T1+(1-al)*T2, Yx=polint3(Tx,Ntab,Ttab,Ytab),   dYx=Zf*Yeq(Tx)-Yx;
            if(fabs(dYx)<0.01*Yx) 
            { *Xf=Mcdm/Tx;
              break;
            } else  { if(dYx>0) {T1=Tx,Y1=Yx;}  else {T2=Tx,Y2=Yx;} }
         }
         break; 
      }  
      else {dY1=dY2; T1=T2; Y1=Y2; *Xf=Mcdm/T2;}        
    }
  }

  if(deltaY==0)  
  {     
      printf("final Yt: %3e \n",Yt);
      return 2.742E8*Mcdm*Yt;
      //2.742E8*Mcdm*Yt
  }  
  else{
    printf("error in relic density calcultion");
    return -1;
  }
}

/*=================================================*/
/*===========  Coscattering 2D  ==================*/
/*=================================================*/

static void XderivLnCosc2(double X, double *Y, double *dYdX)
{
  double coeffCosc,coeffAn,coeff,T,dy1,dy2;
  double vSig0,vSig1;
  double yy[2];
  double dyy[2];

  vSig0=vSigmaStat0(Mcdm/X)/3.8937966E8;//GeV−2 = 0.3894 mb = 3.894E8 pb
  vSig1=vSigmaStat1(Mcdm/X)/3.8937966E8;
  
  T = Mcdm/X;
  Y = yy;
  dYdX = dyy;

  //coeff = -neqF(T,5.11E-4,2)*(1/pow(T,2)/X/sqrt(8*pow(M_PI,3)))*MPlank*sqrt(1/gEff(T))*sqrt(90);
  coeffCosc= -(3*1.202/4)*2*(T/X/sqrt(8*pow(M_PI,7)))*MPlank*sqrt(1/gEff(T))*sqrt(90);  
  coeffAn= -(Mcdm/X/X)*MPlank*sqrt(gEff(T))*sqrt(M_PI/45); 
  // dYdX[0]= coeffCosc*vSig0*(yy[0]-Yeq1(T)*yy[1]/Yeq2(T));
  *dYdX= coeffCosc*vSig0*(Y[0]-Yeq(T));
  // dYdX[1]= -coeff*vSig0*(yy[0]-Yeq1(T)*yy[1]/Yeq2(T))+coeffAn*vSig1*(yy[1]*yy[1]-Yeq2(T)*Yeq2(T));
  dYdX[1]= coeffAn*vSig1*(Y[1]*Y[1]-Yeq2(T)*Yeq2(T));
  //dYdX[1] = 0;
  printf("XLN: dydx1:%3e\tdydx2:%3e\n",dYdX[0],dYdX[1]);
}

static void XderivLnCoscChi(double x, double *Y, double *dYdx)
{
  double y=Y[0];
  double y2 = Y[1];
  double yeq, sqrt_gStar;
  double T,heff,geff;
  double vSig0,vSig1,coeff,coeffAn;
  
  T=Mcdm/x;  
  //yeq=Yeq(T);
  yeq=Yeq(Mcdm/x);
  //if(y<=yeq) {*dYdx=0; return;} else epsY=deltaY/y;
  
  
  vSig0=vSigmaStat0(Mcdm/x)/3.8937966E8;//GeV−2 = 0.3894 mb = 3.894E8 pb
  vSig1=vSigmaStat1(Mcdm/x)/3.8937966E8;//GeV−2 = 0.3894 mb = 3.894E8 pb
  // printf("psiAn:%3e\n",vSig1);
  
  
  coeff = -(3*1.202/4)*2*(T/x/sqrt(8*pow(M_PI,7)))*MPlank*sqrt(1/gEff(T))*sqrt(90)*vSig0;
  //coeff = -neqF(T,5.11E-4,2)*(1/pow(T,2)/x/sqrt(8*pow(M_PI,3)))*MPlank*sqrt(1/gEff(T))*sqrt(90)*vSig0;
  coeffAn= -(Mcdm/x/x)*MPlank*sqrt(gEff(T))*sqrt(M_PI/45); 
  printf("psiAn:%3e\n",vSig1);
  dYdx[0]=coeff*(y-yeq*y2/Yeq2(T));
  // dYdx[1]= coeffAn*vSig1*(y2*y2-Yeq2(T)*Yeq2(T));
  dYdx[1]=-coeff*(y-yeq*y2/Yeq2(T))-1.E10*vSig1*(y2*y2-Yeq2(T)*Yeq2(T));
  //printf("XLN: dydx1:%3e\tdydx2:%3e\n",dYdx[0],dYdx[1]);
}

static void XderivLnCosc2d(double x, double *Y, double*f,double h,double*dfdx,double*dfdy)
{
  int i, n=2;
  double yeq1,yeq2, sqrt_gStar;
  double T,heff,geff;
  double vSig0, vSig1,coeff1,coeff2;
  
  T=Mcdm/x;  
  //yeq=Yeq(T);
  yeq1=Yeq1(T);
  yeq2=Yeq2(T);
  //if(y<=yeq) {*dYdx=0; return;} else epsY=deltaY/y;
  
  vSig0=vSigmaStat0(T)/3.8937966E8;//GeV−2 = 0.3894 mb = 3.894E8 pb
  vSig1=vSigmaStat1(T)/3.8937966E8;//GeV−2 = 0.3894 mb = 3.894E8 pb
    
  //coeff = -(3*1.202/4)*ge*(T/x/sqrt(8*pow(M_PI,7)))*MPlank*sqrt(1/gEff(T))*sqrt(90)*vSig0;
  coeff1 = -neqF(T,5.11E-4,2)*(1/pow(T,2)/x/sqrt(8*pow(M_PI,3)))*MPlank*sqrt(1/gEff(T))*sqrt(90)*vSig0;
  coeff2 = -(Mcdm/x/x)*MPlank*sqrt(gEff(Mcdm/x))*sqrt(M_PI/45)*vSig1;
  //printf("cosc factor: %4e \n",coeff);
  //printf("y difference: %4e\n",(yeq/y));
  f[0] = coeff1*(Y[0]-yeq1);
  f[1] = coeff2*(Y[1]*Y[1]-yeq2*yeq2)-coeff1*(Y[0]-yeq1);

  if(dfdx) for(i=0;i<n;i++) dfdx[i]=0;
    if(dfdy)
    {
      dfdy[0*n+0]= coeff1;
      dfdy[0*n+1]=  0;
      dfdy[1*n+0]=  -coeff1;
      dfdy[1*n+1]= 2*coeff2*Y[1];
    }
  }

// ***************************END　of DERIVARIVE ********************************

static int odeintTest(double * ystart, int nvar, double x1, double x2, double eps, 
         double h1, void (*derivs)(double,double *,double *))
{
   int nstp,i;
   double x,hnext,hdid,h;

   double *yscal,*y,*dydx;

   int MAXSTP=25;

   for (i=0;i<nvar;i++){
     //y[i]=ystart[i];
     printf("ystart:%4e\n",ystart[i]);
   }
   
   return 1;

}

double darkOmegaCosc2d(double * Xf,  double (*f0)(double),double (*f1)(double))
{
  double Xt=9;
  //double Yt[2];
  double Z1=1.1;
  double Z2=10,Zf=2.5; 
  double y;
  double yscale[2]= {1,1};
  int i;
  double h;
  
  if(f0) vSigmaStat0=f0; else vSigmaStat0=vSigmaZero;
  if(f1) vSigmaStat1=f1; else vSigmaStat1=vSigmaZero;
  
  MassCut=4*Mcdm;

  int Nt=25;
  
  Ytab=realloc(Ytab,sizeof(double)*Nt);
  Ttab=realloc(Ttab,sizeof(double)*Nt);
  Ntab=0;

  Mcdm2=Mcdm;
  for(i=0;i<NC;i++) if(Z4ch(inP[i])==1) 
  {  Mcdm1=inMass[i];
     if(Mcdm1>Mcdm)
     {  Mcdm2 = inMass[i];
        Mcdm1 = Mcdm;
     }
  }   
  printf("Mcdm:%e,\tMcdm1:%e,\tMcdm2:%e\n",Mcdm,Mcdm1,Mcdm2);

  if(CDM1==NULL) fracCDM2=1; else
  if(CDM2==NULL) fracCDM2=0; else 
  if(Mcdm1<Mcdm2)fracCDM2=0; else fracCDM2=1;

  //Yt=  darkOmega1Ext1(&Xt, Z1, (Z1-1)/5);
  //double Yt = Yeq(Mcdm/Xt)*1.1;
  //Yt[1] = Yeq2(Mcdm/Xt);
  double Yt[2] = {Yeq1(Mcdm/Xt)*1.01,Yeq2(Mcdm/Xt)};
  //printf("initial x: %3e, initial y1: %3e \n",Xt,Yt[0]);
  

  //if(Yt[0]<0||FError) { return -1;}
     
  
  Tstart=Mcdm/Xt;
  Ntab=1;
  Ttab[0]=Tstart;
  Ytab[0]= Yt[0];
  Tend=Tstart;
  printf("X:%3e,\t Y1: %4e\t Y2:%4e \n",Xt,Yt[0],Yt[1]);
         
  for(i=0;;i++)
  { 
    double X2,Tbeg;
    double yeq=Yeq(Mcdm/Xt);
    //printf("X:%3e,\t y1diff: %4e\t y2diff:%3e\n",Xt,(Yt[0])/(Yeq(Mcdm/Xt)),(Yt[1])/(Yeq2(Mcdm/Xt)));
  


    if(Tend<1.E-3) break;
    Tbeg=Tend;       
    Tend/=1.1;                                 
    Xt=Mcdm/(Tbeg);
    X2=Mcdm/(Tend); 
    h = (X2-Xt)/2;
    //y=log(Yt);
    //odeint(Yt,2, Xt , X2 , 1.E-3, (X2-Xt)/2, XderivLnCoscChi); 
    stifbs(1,Xt,X2,2,Yt,yscale,1.E-3, &h, XderivLnCosc2d);
    Xt=X2;
    //if(odeint(&Yt,1 ,s3_t , s3_2 , 1.E-3, (s3_2-s3_t)/2, &XderivLnExt)){ printf("problem in solving diff.equation\n"); return -1;}
    //if(!isfinite(Yt)||FError)  return -1;
    if(Ntab>=Nt)
    { Nt+=20;   
      Ytab=realloc(Ytab,sizeof(double)*Nt);
      Ttab=realloc(Ttab,sizeof(double)*Nt);
    }      
    Ytab[Ntab]=Yt[0];
    Ttab[Ntab]=Tend;
    Ntab++;  
    Tbeg=Tend; 
    // printf("X:%3e,\t y1diff: %4e \n",Xt,Ytab[Ntab-1]/Ytab[Ntab-2]); 
    printf("X:%3e,\t Y1: %4e\t Y2:%4e \n",Xt,Yt[0],Yt[1]);                                         
  }  
  
  
  if(Xf) 
  {  double T1,T2,Y1,Y2,dY2,dY1;
     T1=Ttab[0];
     Y1=Ytab[0];
     dY1=Zf*Yeq1(T1)-Y1;  
     *Xf=Mcdm/T1;
     for(i=1;i<Ntab;i++)            
     { T2=Ttab[i];
       Y2=Ytab[i]; 
       dY2=Zf*Yeq1(T2)-Y2;
       if(dY2<0)
       {          
         for(;;)
         {  double al,Tx,Yx,dYx,Xx;
            al=dY2/(dY2-dY1);
            Tx=al*T1+(1-al)*T2, Yx=polint3(Tx,Ntab,Ttab,Ytab),   dYx=Zf*Yeq1(Tx)-Yx;
            if(fabs(dYx)<0.01*Yx) 
            { *Xf=Mcdm/Tx;
              break;
            } else  { if(dYx>0) {T1=Tx,Y1=Yx;}  else {T2=Tx,Y2=Yx;} }
         }
         break; 
      }  
      else {dY1=dY2; T1=T2; Y1=Y2; *Xf=Mcdm/T2;}        
    }
  }
  

  if(deltaY==0)  
  {     
      return 2.742E8*Mcdm*(Yt[0]);
      //2.742E8*Mcdm*Yt
  }  
  else{
    printf("error in relic density calcultion");
    return -1;
  }
}

double darkOmegaCosc2(double * Xf,  double (*f0)(double),double (*f1)(double))
{
  double Xt=12;
  //double Yt[2];
  double Z1=1.1;
  double Z2=10,Zf=2.5; 
  double y;
  int i;
  
  if(f0) vSigmaStat0=f0; else vSigmaStat0=vSigmaZero;
  if(f1) vSigmaStat1=f1; else vSigmaStat1=vSigmaZero;
  
  MassCut=4*Mcdm;

  int Nt=25;
  
  Ytab=realloc(Ytab,sizeof(double)*Nt);
  Ttab=realloc(Ttab,sizeof(double)*Nt);
  Ntab=0;

  Mcdm2=Mcdm;
  for(i=0;i<NC;i++) if(Z4ch(inP[i])==1) 
  {  Mcdm1=inMass[i];
     if(Mcdm1>Mcdm)
     {  Mcdm2 = inMass[i];
        Mcdm1 = Mcdm;
     }
  }   
  printf("Mcdm:%e,\tMcdm1:%e,\tMcdm2:%e\n",Mcdm,Mcdm1,Mcdm2);

  if(CDM1==NULL) fracCDM2=1; else
  if(CDM2==NULL) fracCDM2=0; else 
  if(Mcdm1<Mcdm2)fracCDM2=0; else fracCDM2=1;

  //Yt=  darkOmega1Ext1(&Xt, Z1, (Z1-1)/5);
  double Yt = Yeq(Mcdm/Xt)*1.1;
  //Yt[1] = Yeq2(Mcdm/Xt);
  //double Yt[2] = {Yeq(Mcdm/Xt)*1.1,Yeq2(Mcdm/Xt)};
  //printf("initial x: %3e, initial y1: %3e \n",Xt,Yt[0]);
  

  //if(Yt[0]<0||FError) { return -1;}
     
  
  Tstart=Mcdm/Xt;
  Ntab=1;
  Ttab[0]=Tstart;
  Ytab[0]= Yt;
  Tend=Tstart;
         
  for(i=0; ;i++)
  { 
    double X2,Tbeg;
    double yeq=Yeq(Mcdm/Xt);
    //printf("X:%3e,\t y1diff: %4e\t y2diff:%3e\n",Xt,(Yt[0])/(Yeq(Mcdm/Xt)),(Yt[1])/(Yeq2(Mcdm/Xt)));
  


    if(Tend<1.E-3) break;
    Tbeg=Tend;       
    Tend/=1.2;                                 
    Xt=Mcdm/(Tbeg);
    X2=Mcdm/(Tend); 
    //y=log(Yt);
    odeint(&Yt,1, Xt , X2 , 1.E-3, (X2-Xt)/2, &XderivLnCoscChi); 
    //Yt=exp(y);
    //Yt[0] = Yeq(Mcdm/Xt)*1.;
    //Yt[1] = Yeq2(Mcdm/Xt);
    //printf("current Y1:%4e\t Y2:%4e\n",Yt[0],Yt[1]);
    Xt=X2;
    //if(odeint(&Yt,1 ,s3_t , s3_2 , 1.E-3, (s3_2-s3_t)/2, &XderivLnExt)){ printf("problem in solving diff.equation\n"); return -1;}
    if(!isfinite(Yt)||FError)  return -1;
    if(Ntab>=Nt)
    { Nt+=20;   
      Ytab=realloc(Ytab,sizeof(double)*Nt);
      Ttab=realloc(Ttab,sizeof(double)*Nt);
    }      
    Ytab[Ntab]=Yt;
    Ttab[Ntab]=Tend;
    Ntab++;  
    Tbeg=Tend; 
    printf("X:%3e,\t y1diff: %4e \n",Xt,Ytab[Ntab-1]/Ytab[Ntab-2]);                                          
  }  
  
  
  if(Xf) 
  {  double T1,T2,Y1,Y2,dY2,dY1;
     T1=Ttab[0];
     Y1=Ytab[0];
     dY1=Zf*Yeq(T1)-Y1;  
     *Xf=Mcdm/T1;
     for(i=1;i<Ntab;i++)            
     { T2=Ttab[i];
       Y2=Ytab[i]; 
       dY2=Zf*Yeq(T2)-Y2;
       if(dY2<0)
       {          
         for(;;)
         {  double al,Tx,Yx,dYx,Xx;
            al=dY2/(dY2-dY1);
            Tx=al*T1+(1-al)*T2, Yx=polint3(Tx,Ntab,Ttab,Ytab),   dYx=Zf*Yeq(Tx)-Yx;
            if(fabs(dYx)<0.01*Yx) 
            { *Xf=Mcdm/Tx;
              break;
            } else  { if(dYx>0) {T1=Tx,Y1=Yx;}  else {T2=Tx,Y2=Yx;} }
         }
         break; 
      }  
      else {dY1=dY2; T1=T2; Y1=Y2; *Xf=Mcdm/T2;}        
    }
  }
  

  if(deltaY==0)  
  {     
      return 2.742E8*Mcdm*(Yt);
      //2.742E8*Mcdm*Yt
  }  
  else{
    printf("error in relic density calcultion");
    return -1;
  }
}


// ===========  Z4  ==================

static double geff1_(double T)
{ 

  double massCut=Mcdm1;
  if(Beps>0)    massCut-=T*log(Beps); else  massCut+=1.E20;

   double sum=0,t; int l;
   for(l=0;l<NC;l++) if(!feebleParticles[oddPpos[sort[l]]]) 
   { int k=sort[l];
     if(Z4ch(inP[k])==1) 
     { double bsk2; 
       double M=inMass[k];
       if(M>massCut) continue;
       t=T/M;
       if(t<0.1) bsk2=K2pol(t)*exp((Mcdm1-M)/T)*sqrt(M_PI*t/2);
        else     bsk2=bessK2(1/t)*exp(Mcdm1/T);
       sum+=inG[k]*M*M*bsk2;
     }
   }      
   return sum;
}

static double geff2_(double T)
{ 
   double massCut=Mcdm2;
   if(Beps>0)  massCut-=T*log(Beps); else  massCut+=1.E20;
   double sum=0,t; int l;
   for(l=0;l<NC;l++) if(!feebleParticles[oddPpos[sort[l]]])
   { int k=sort[l];
     if(Z4ch(inP[k])==2) 
     { double bsk2; 
       double M=inMass[k];
       if(M>massCut) continue;
       t=T/M;
       if(t<0.1) bsk2=K2pol(t)*exp(-1/t+Mcdm2/T)*sqrt(M_PI*t/2);
        else     bsk2=bessK2(1/t)*exp(Mcdm2/T);
       sum+=inG[k]*M*M*bsk2;
     }
   }  
   return sum;
}


static double McdmSum;
double Beps=1.E-4;




double Yeq1(double T)
{  double heff,s;
   s=2*M_PI*M_PI*T*T*T*hEff(T)/45;
   return  pow(Mcdm*T/(2*M_PI),1.5)*2*exp(-Mcdm/T)/s;;
}


double Yeq2(double T)
{  double s;
   s=2*M_PI*M_PI*T*T*T*hEff(T)/45;
   //return  (T/(2*M_PI*M_PI*s))*geff2_(T)*exp(-Mcdm2/T); 
   return  pow(Mcdm2*T/(2*M_PI),1.5)*2*exp(-Mcdm2/T)/s;
}


static double Y1SQ_Y2(double T)
{ 
  double s,X1,X2,g1_,g2_,res;
  X1=Mcdm1/T;
  X2=Mcdm2/T;
  if(X2-2*X1>500) return 0;
  s=2*M_PI*M_PI*T*T*T*hEff(T)/45;
  
  g1_=geff1_(T);
  g2_=geff2_(T);
      
  res = g1_/g2_*exp(X2-2*X1)*T*g1_/(2*M_PI*M_PI*s);
//  if(!isfinite(res)) { printf("T=%E  g1_=%E g2_=%E X1=%E X2=%E\n", T,g1_,g2_,X1,X2); exit(0);}
  return res;
}                          

static double Y2SQ_Y1(double T)
{ 
  double s,X1,X2,g1_,g2_,res;
  X1=Mcdm1/T;
  X2=Mcdm2/T;
  if(X1-2*X2>500) return 0;
  s=2*M_PI*M_PI*T*T*T*hEff(T)/45;
  
  g1_=geff1_(T);
  g2_=geff2_(T);
      
  res = g1_/g2_*exp(X1-2*X2)*T*g1_/(2*M_PI*M_PI*s);
//  if(!isfinite(res)) { printf("T=%E  g1_=%E g2_=%E X1=%E X2=%E\n", T,g1_,g2_,X1,X2); exit(0);}
  return res;
}                          

