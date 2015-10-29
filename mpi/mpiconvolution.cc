#include "mpiconvolution.h"

using namespace utils;

namespace fftwpp {

void ImplicitConvolution2MPI::convolve(Complex **F, multiplier *pmult,
                                       std::vector<unsigned int>& index,
                                       unsigned int offset)
{
  for(unsigned int a=0; a < A; ++a) {
    Complex *f=F[a]+offset;
    Complex *u=U2[a];
    xfftpad->expand(f,u);
    if(a > 0) T->wait0();
    xfftpad->Backwards->fft(f);
    if(a > 0) T->wait1();
    T->itranspose(f,false,true);
    xfftpad->Backwards->fft(u);
    T->wait();
    T->itranspose(u,false,true);
  }
      
  subconvolution(F,pmult,index,0,d.x,d.Y,offset);
  T->wait();
  T->itranspose(F[0]+offset,true,false);
  subconvolution(U2,pmult,index,1,d.x,d.Y);
  T->wait();
  for(unsigned int b=1; b < B; ++b)
    T->transpose(F[b]+offset,true,false);
    
  for(unsigned int b=0; b < B; ++b) {
    Complex *f=F[b]+offset;
    Complex *u=U2[b];
    T->itranspose(u,true,false);
    xfftpad->Forwards->fft(f);
    T->wait();
    xfftpad->Forwards->fft(u);
    xfftpad->reduce(f,u);
  }
}
  
void ImplicitHConvolution2MPI::convolve(Complex **F, realmultiplier *pmult,
                                        bool symmetrize,
                                        std::vector<unsigned int>&index,
                                        unsigned int offset)
{
  if(d.y0 > 0) symmetrize=false;

  for(unsigned int a=0; a < A; ++a) {
    Complex *f=F[a]+offset;
    Complex *u=U2[a];
    if(symmetrize)
      HermitianSymmetrizeX(mx,d.y,mx-xcompact,f);
    xfftpad->expand(f,u);
    xfftpad->Backwards->fft(f);
    if(a > 0) T->wait0();
    if(a > 0) U->wait0();
    xfftpad->Backwards1(f,u);
    if(a > 0) T->wait1();
    T->itranspose(f,false,true);
    xfftpad->Backwards->fft(u);
    if(a > 0) U->wait1();
    U->itranspose(u,false,true);
  }
  
      
  T->wait();
  subconvolution(F,pmult,index,xfftpad->findex,d.x,d.Y,offset);
  U->wait0();
  for(unsigned int b=0; b < B; ++b) {
    if(b > 0) T->wait();
    T->itranspose(F[b]+offset,true,false);
  }
  U->wait1();
  subconvolution(U2,pmult,index,xfftpad->uindex,du.x,du.Y);
  T->wait();
    
  for(unsigned int b=0; b < B; ++b) {
    Complex *f=F[b]+offset;
    Complex *u=U2[b];
    U->itranspose(u,true,false);
    xfftpad->Forwards0(f);
    U->wait();
    xfftpad->Forwards1(f,u);
    xfftpad->Forwards->fft(f);
    xfftpad->Forwards->fft(u);
    xfftpad->reduce(f,u);
  }
}

void ImplicitConvolution3MPI::convolve(Complex **F, multiplier *pmult,
                                       std::vector<unsigned int>& index,
                                       unsigned int offset) 
{
  for(unsigned int a=0; a < A; ++a) {
    Complex *f=F[a]+offset;
    Complex *u=U3[a];
    xfftpad->expand(f,u);
    if(a > 0 && d.y < d.Y) T->wait0();
    xfftpad->Backwards->fft(f);
    if(T) {
      if(a > 0) T->wait1();
      T->itranspose(f,false,true);
    }
    xfftpad->Backwards->fft(u);
    if(T) {
      T->wait();
      T->itranspose(u,false,true);
    }
  }
      
  unsigned int stride=d.Y*d.z;
    
  subconvolution(F,pmult,index,0,d.x,stride,offset);
  if(T) {
    T->wait();
    T->itranspose(F[0]+offset,true,false);
  }
  subconvolution(U3,pmult,index,1,d.x,stride);
  if(T) {
    T->wait();
    for(unsigned int b=1; b < B; ++b)
      T->transpose(F[b]+offset,true,false);
  }
    
  for(unsigned int b=0; b < B; ++b) {
    Complex *f=F[b]+offset;
    Complex *u=U3[b];
    if(T)
      T->itranspose(u,true,false);
    xfftpad->Forwards->fft(f);
    if(T)
      T->wait();
    xfftpad->Forwards->fft(u);
    xfftpad->reduce(f,u);
  }
}

// Enforce 3D Hermiticity using given (x,y > 0,z=0) and (x >= 0,y=0,z=0) data.
// u0 is an optional work array of size nu=d.X-!xcompact.
void HermitianSymmetrizeXYMPI(unsigned int mx, unsigned int my,
                              split3& d, bool xcompact, bool ycompact,
                              Complex *f, unsigned int nu, Complex *u0)
{
  int rank,size;
  MPI_Comm_size(d.communicator,&size);
  if(size == 1) {
    if(mx > 0 && my > 0 && d.Z > 0)
      HermitianSymmetrizeXY(mx,my,d.Z,mx-xcompact,my-ycompact,f);
    return;
  }
  unsigned int xextra=!xcompact;
  unsigned int yextra=!ycompact;
  unsigned int yorigin=my-ycompact;
  unsigned int nx=d.X-xextra;
  unsigned int y0=d.xy.y0;
  unsigned int dy=d.xy.y;
  unsigned int j0=y0 == 0 ? yextra : 0;
  unsigned int start=(yorigin > y0) ? yorigin-y0 : 0;
  
  if(d.XYplane == NULL) {
    d.XYplane=new MPI_Comm;
    MPI_Comm_split(d.communicator,d.z0 == 0,0,d.XYplane);
    if(d.z0 != 0) return;
    MPI_Comm_rank(*d.XYplane,&rank);
    MPI_Comm_size(*d.XYplane,&size);
    d.reflect=new int[dy];
    range *indices=new range[size];
    indices[rank].n=dy;
    indices[rank].start=y0;
    MPI_Allgather(MPI_IN_PLACE,0,MPI_INT,indices,
                  sizeof(range)/sizeof(MPI_INT),MPI_INT,*d.XYplane);
  
    if(rank == 0) {
      int *process=new int[d.Y];
      for(int p=0; p < size; ++p) {
        unsigned int stop=indices[p].start+indices[p].n;
        for(unsigned int j=indices[p].start; j < stop; ++j)
          process[j]=p;
      }
    
      for(unsigned int j=j0; j < dy; ++j)
        d.reflect[j]=process[2*yorigin-y0-j];
      for(int p=1; p < size; ++p) {
        for(unsigned int j=indices[p].start == 0 ? yextra : 0; j < indices[p].n;
            ++j)
          MPI_Send(process+2*yorigin-indices[p].start-j,1,MPI_INT,p,j,
                   *d.XYplane);
      }
      delete [] process;
    } else {
      for(unsigned int j=0; j < dy; ++j)
        MPI_Recv(d.reflect+j,1,MPI_INT,0,j,*d.XYplane,MPI_STATUS_IGNORE);
    }
    delete [] indices;
  }
  if(d.z0 != 0) return;
  MPI_Comm_rank(*d.XYplane,&rank);

  Complex *u=(nu < nx) ? ComplexAlign(nx) : u0;
  unsigned int stride=dy*d.z;
  for(unsigned int j=start; j < dy; ++j) {
    for(unsigned int i=0; i < nx; ++i)
      u[i]=conj(f[stride*(d.X-1-i)+d.z*j]);
    int J=d.reflect[j];
    if(J != rank)
      MPI_Send(u,2*nx,MPI_DOUBLE,J,0,*d.XYplane);
    else {
      if(y0+j != yorigin) {
        int offset=d.z*(2*(yorigin-y0)-j);
        for(unsigned int i=0; i < nx; ++i) {
          unsigned int N=stride*(i+xextra)+offset;
          if(N < d.n)
            f[N]=u[i];
          else {
            if(rank == 0)
              std::cerr << "Invalid index in HermitianSymmetrizeXYMPI."
                        << std::endl;
            exit(-1);
          }
        }
      } else {
        unsigned int origin=stride*(mx-xcompact)+d.z*j;
        f[origin].im=0.0;
        unsigned int mxstride=mx*stride;
        for(unsigned int i=stride; i < mxstride; i += stride)
          f[origin-i]=conj(f[origin+i]);
      }
    }
  }

  for(unsigned int j=std::min(dy,start); j-- > j0;) {
    int J=d.reflect[j];
    if(J != rank) {
      MPI_Recv(u,2*nx,MPI_DOUBLE,J,0,*d.XYplane,MPI_STATUS_IGNORE);
      for(unsigned int i=0; i < nx; ++i)
        f[stride*(i+xextra)+d.z*j]=u[i];
    }
  }
  
  if(nu < nx) deleteAlign(u);
}

void ImplicitHConvolution3MPI::convolve(Complex **F, realmultiplier *pmult,
                                        bool symmetrize,
                                        std::vector<unsigned int>&index,
                                        unsigned int offset)
{
  for(unsigned int a=0; a < A; ++a) {
    Complex *f=F[a]+offset;
    Complex *u=U3[a];
    if(symmetrize)
      HermitianSymmetrizeXYMPI(mx,my,d,xcompact,ycompact,f,du.n,u);
    xfftpad->expand(f,u);
    xfftpad->Backwards->fft(f);
    if(U && a > 0) U->wait0();
    xfftpad->Backwards1(f,u);
    if(T) {
      if(a > 0) T->wait1();
      T->itranspose(f,false,true);
    }
    xfftpad->Backwards->fft(u);
    if(U) {
      if(a > 0) U->wait1();
      U->itranspose(u,false,true);
      T->wait0();
    }
  }

  if(T) T->wait1();
  subconvolution(F,pmult,index,xfftpad->findex,d.x,d.Y*d.z,offset);
  if(U) {
    U->wait0();
    for(unsigned int b=0; b < B; ++b) {
      if(b > 0) T->wait();
      T->itranspose(F[b]+offset,true,false);
    }
    U->wait1();
  }
  subconvolution(U3,pmult,index,xfftpad->uindex,du.x,du.Y*du.z);
  if(T) T->wait();
    
  for(unsigned int b=0; b < B; ++b) {
    Complex *f=F[b]+offset;
    Complex *u=U3[b];
    if(U)
      U->itranspose(u,true,false);
    xfftpad->Forwards0(f);
    if(U) 
      U->wait();
    xfftpad->Forwards1(f,u);
    xfftpad->Forwards->fft(f);
    xfftpad->Forwards->fft(u);
    xfftpad->reduce(f,u);
  }
}

} // namespace fftwpp
