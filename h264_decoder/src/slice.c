//============================================================================//
//    H.264 High Level Synthesis Benchmark
//    Copyright (c) <2016>
//    <University of Illinois at Urbana-Champaign>
//    All rights reserved.
//
//    Developed by:
//
//    <ES CAD Group>
//        <University of Illinois at Urbana-Champaign>
//        <http://dchen.ece.illinois.edu/>
//
//    <Hardware Research Group>
//        <Advanced Digital Sciences Center>
//        <http://adsc.illinois.edu/>
//============================================================================//

#include "global.h"
#include "intrapred.h"
#include "vlc.h"
#include "cavlc.h"
#include "mathfunc.h"
#include "residual.h"
#include "interpred.h"
#include <string.h>

void write_luma
(
 unsigned char pred[4][4],
 int rMb[4][4],
 unsigned char Sluma[PicWidthInSamplesL][FrameHeightInSampleL],
 int startx,
 int starty,
 unsigned char skip)
{
#pragma HLS ARRAY_PARTITION variable=rMb complete dim=1
#pragma HLS ARRAY_PARTITION variable=rMb complete dim=2

// Disable HLS directives on function parameters when the function is a top-module for SDSoC/HLS.
//#pragma HLS ARRAY_PARTITION variable=pred complete dim=1
//#pragma HLS ARRAY_PARTITION variable=pred complete dim=2
  int i,j;

  for(i=0;i<4;i++)
    for(j=0;j<4;j++)
    {
      #pragma HLS PIPELINE rewind
      Sluma[startx+i][starty+j]=Clip1y( skip*rMb[i][j]+pred[i][j]);
    }


}

void write_Chroma
(
 unsigned char pred[4][4],
 int rMb[4][4],
 unsigned char SChroma[PicWidthInSamplesC][FrameHeightInSampleC],
 int startx,
 int starty,
 unsigned char skip
 )
{
#pragma HLS ARRAY_PARTITION variable=rMb complete dim=1
#pragma HLS ARRAY_PARTITION variable=rMb complete dim=2

// Disable HLS directives on function parameters when the function is a top-module for SDSoC/HLS.
//#pragma HLS ARRAY_PARTITION variable=pred complete dim=1
//#pragma HLS ARRAY_PARTITION variable=pred complete dim=2
  int i,j;

  for(i=0;i<4;i++)
    for(j=0;j<4;j++)
    {
      #pragma HLS PIPELINE rewind
      SChroma[startx+i][starty+j]=Clip1y((skip==0)*rMb[i][j]+pred[i][j]);
    }
}

#pragma SDS data mem_attribute (nalu_buf:PHYSICAL_CONTIGUOUS)
#pragma SDS data access_pattern(nalu_buf:RANDOM)
#pragma SDS data zero_copy(nalu_buf[0:MAXNALBUFFERSIZE])

#pragma SDS data mem_attribute (PIC:PHYSICAL_CONTIGUOUS)
#pragma SDS data access_pattern(PIC:RANDOM)
#pragma SDS data zero_copy(PIC[0:MAX_REFERENCE_PICTURES])

#pragma SDS data mem_attribute (PIC_Sluma:PHYSICAL_CONTIGUOUS)
#pragma SDS data access_pattern(PIC_Sluma:RANDOM)
#pragma SDS data zero_copy(PIC_Sluma[0:MAX_REFERENCE_PICTURES][0:PicWidthInSamplesL][0:FrameHeightInSampleL])

#pragma SDS data mem_attribute (PIC_SChroma_0:PHYSICAL_CONTIGUOUS)
#pragma SDS data access_pattern(PIC_SChroma_0:RANDOM)
#pragma SDS data zero_copy(PIC_SChroma_0[0:MAX_REFERENCE_PICTURES][0:PicWidthInSamplesC][0:FrameHeightInSampleC])

#pragma SDS data mem_attribute (PIC_SChroma_1:PHYSICAL_CONTIGUOUS)
#pragma SDS data access_pattern(PIC_SChroma_1:RANDOM)
#pragma SDS data zero_copy(PIC_SChroma_1[0:MAX_REFERENCE_PICTURES][0:PicWidthInSamplesC][0:FrameHeightInSampleC])

#pragma SDS data access_pattern(Imode:RANDOM)
#pragma SDS data zero_copy(Imode[0:PicWidthInMBs][0:FrameHeightInMbs])

#pragma SDS data access_pattern(IntraPredMode:RANDOM)
#pragma SDS data zero_copy(IntraPredMode[0:PicWidthInMBs*4][0:FrameHeightInMbs*4])

#pragma SDS data access_pattern(NzLuma:RANDOM)
#pragma SDS data zero_copy(NzLuma[0:PicWidthInMBs*4][0:FrameHeightInMbs*4])

void process_luma(
    unsigned char x,
    unsigned char y,
    unsigned char k,
    int mbaddrx,
    int mbaddry,
    unsigned char CodedPatternLuma,
    NALU_t* nalu,
    unsigned char *nalu_buf,
	unsigned long int *nalu_bit_offset,
    int tmpImode,
    int coeffDCL,
    StorablePicture PIC[MAX_REFERENCE_PICTURES],
    unsigned char PIC_Sluma[MAX_REFERENCE_PICTURES][PicWidthInSamplesL][FrameHeightInSampleL],
    unsigned char PIC_SChroma_0[MAX_REFERENCE_PICTURES][PicWidthInSamplesC][FrameHeightInSampleC],
    unsigned char PIC_SChroma_1[MAX_REFERENCE_PICTURES][PicWidthInSamplesC][FrameHeightInSampleC],
    unsigned char Imode[PicWidthInMBs][FrameHeightInMbs],
    char IntraPredMode[PicWidthInMBs*4][FrameHeightInMbs*4],
    unsigned char NzLuma[PicWidthInMBs*4][FrameHeightInMbs*4],
    ImageParameters* img,
    unsigned char predL[4][4],
    char qPm6,
    char qPy,
    char temp1l,
    char temp2l,
    char temp3l,
    char refidx0,
    char refidx1,
    int mvd0[2],
    int mvd1[2],
    unsigned char intra4x4predmode
    )
{
  int nC = 0;
  int i = 0, j = 0;

  int coeffACL[4][4] = { { 0 } };
#pragma HLS ARRAY_PARTITION variable=coeffACL complete dim=1
#pragma HLS ARRAY_PARTITION variable=coeffACL complete dim=2
  unsigned char predL4x4[4][4] = { { 0 } };
#pragma HLS ARRAY_PARTITION variable=predL4x4 complete dim=1
#pragma HLS ARRAY_PARTITION variable=coeffACL complete dim=2
  int xint0 = 0, yint0 = 0, xfrac0 = 0, yfrac0 = 0;
  int xint1 = 0, yint1 = 0, xfrac1 = 0, yfrac1 = 0;

  int rMbL[4][4] = { { 0 } };
  unsigned char avaimode = 0;

  int tmpidx0 = 0;
  int tmpidx1 = 0;
#pragma HLS ARRAY_PARTITION variable=rMbL complete dim=1
#pragma HLS ARRAY_PARTITION variable=rMbL complete dim=2

// Disable HLS directives on function parameters when the function is a top-module for SDSoC/HLS.
//#pragma HLS ARRAY_PARTITION variable=predL complete dim=1
//#pragma HLS ARRAY_PARTITION variable=predL complete dim=2

  unsigned char inter_temp0[9][9] = { { 0 } };
  unsigned char inter_temp1[9][9] = { { 0 } };

#pragma HLS ARRAY_PARTITION variable=inter_temp0 complete dim=1
#pragma HLS ARRAY_PARTITION variable=inter_temp0 complete dim=2

#pragma HLS ARRAY_PARTITION variable=inter_temp1 complete dim=1
#pragma HLS ARRAY_PARTITION variable=inter_temp1 complete dim=2

#pragma HLS ARRAY_PARTITION variable=mvd0 complete dim=1
#pragma HLS ARRAY_PARTITION variable=mvd1 complete dim=1

  //processing luma component of each block
  if(CodedPatternLuma& (1<<(k/4)))
  {
    nC=nc_Luma(Imode,NzLuma,mbaddrx*4+x,mbaddry*4+y);
    NzLuma[mbaddrx*4+x][mbaddry*4+y]=residual_block_cavlc_16(coeffACL,nalu,nalu_buf, nalu_bit_offset,(tmpImode==INTRA16x16),15,nC);
    //quant
  }
  else if(tmpImode==INTRA16x16)
  {
    NzLuma[mbaddrx*4+x][mbaddry*4+y]=0;
    for(i=0;i<4;i++)
      #pragma HLS PIPELINE
      for(j=0;j<4;j++)
      {
        coeffACL[i][j]=0;
      }
  }
  else
  {
    NzLuma[mbaddrx*4+x][mbaddry*4+y]=0;
  }

  if( (CodedPatternLuma& (1<<(k/4))) || tmpImode==INTRA16x16)
    scale_residual4x4_and_trans_inverse(qPy, qPm6, temp1l, temp2l, temp3l, coeffACL, rMbL, coeffDCL,(tmpImode==INTRA16x16)); /* 8.5.12.1 */

  if(tmpImode==INTRA4x4)
  {
    avaimode=( (mbaddrx*4+x)>0)*2+((mbaddry*4+y)>0);
    predict_intra4x4_luma_NonField(predL4x4,PIC_Sluma[img->mem_idx],intra4x4predmode,avaimode,(mbaddrx*4+x)*4,(mbaddry*4+y)*4,k);
    write_luma(predL4x4,rMbL,PIC_Sluma[img->mem_idx],(mbaddrx*4+x)*4,(mbaddry*4+y)*4, (CodedPatternLuma& (1<<(k/4))) || tmpImode==INTRA16x16);
  }
  else if(tmpImode==INTRA16x16)
  {
    write_luma(predL,rMbL,PIC_Sluma[img->mem_idx],(mbaddrx*4+x)*4,(mbaddry*4+y)*4, (CodedPatternLuma& (1<<(k/4))) || tmpImode==INTRA16x16);
  }
  else
  {
    if(refidx0>=0 && refidx1<0)
    {
      xint0=(mbaddrx*4+x)*4+(mvd0[0]>>2);
      yint0=(mbaddry*4+y)*4+(mvd0[1]>>2);
      xfrac0=(mvd0[0]&0x03);
      yfrac0=(mvd0[1]&0x03);
      tmpidx0=img->list0[(refidx0>=0)*refidx0];
    }
    else if(refidx1>=0 && refidx0<0)
    {
      xint0=(mbaddrx*4+x)*4+(mvd1[0]>>2);
      yint0=(mbaddry*4+y)*4+(mvd1[1]>>2);
      xfrac0=(mvd1[0]&0x03);
      yfrac0=(mvd1[1]&0x03);
      tmpidx0=img->list1[(refidx1>=0)*refidx1];

    }
    else if(refidx1>=0 && refidx0>=0)
    {
      xint1=(mbaddrx*4+x)*4+(mvd1[0]>>2);
      yint1=(mbaddry*4+y)*4+(mvd1[1]>>2);
      xfrac1=(mvd1[0]&0x03);
      yfrac1=(mvd1[1]&0x03);
      tmpidx1=img->list1[(refidx1>=0)*refidx1];

      xint0=(mbaddrx*4+x)*4+(mvd0[0]>>2);
      yint0=(mbaddry*4+y)*4+(mvd0[1]>>2);
      xfrac0=(mvd0[0]&0x03);
      yfrac0=(mvd0[1]&0x03);
      tmpidx0=img->list0[(refidx0>=0)*refidx0];
    }
    if(refidx0>=0 && refidx1>=0 && xfrac0==0 && yfrac0==0 && xfrac1==0 && yfrac1==0 )
    {

      inter_luma_double_bizero_skip
        (
         PIC_Sluma[tmpidx0],
         PIC_Sluma[tmpidx1],
         PIC_Sluma[img->mem_idx],
         (mbaddrx*4+x)*4,
         (mbaddry*4+y)*4,
         xint0,
         yint0,
         xint1,
         yint1,
         rMbL,
         (CodedPatternLuma&(1<<(k/4)))!=0
        );
    }
    else if(refidx0>=0 && refidx1>=0)
    {
      copy_comp(PIC_Sluma[tmpidx0], xint0, yint0,xfrac0,yfrac0, inter_temp0);
      copy_comp(PIC_Sluma[tmpidx1], xint1, yint1,xfrac1,yfrac1, inter_temp1);

      inter_luma_double_skip
        (
         PIC_Sluma[img->mem_idx],
         (mbaddrx*4+x)*4,
         (mbaddry*4+y)*4,
         xfrac0,
         yfrac0,
         xfrac1,
         yfrac1,
         (xfrac0==3 && yfrac0!=0),
         (yfrac0==3 && xfrac0!=0),
         (xfrac1==3 && yfrac1!=0),
         (yfrac1==3 && xfrac1!=0),
         inter_temp0,
         inter_temp1,
         rMbL,
         (CodedPatternLuma&(1<<(k/4)))!=0
        );

    }
    else
    {
      copy_comp(PIC_Sluma[tmpidx0], xint0, yint0,xfrac0,yfrac0,inter_temp0);
      inter_luma_single
        (
         PIC_Sluma[img->mem_idx],
         rMbL,
         (mbaddrx*4+x)*4,
         (mbaddry*4+y)*4,
         xfrac0,
         yfrac0,
         (xfrac0==3 && yfrac0!=0),
         (yfrac0==3 && xfrac0!=0),
         inter_temp0,
         (CodedPatternLuma&(1<<(k/4)))!=0
        );
    }
  }
}

#pragma SDS data mem_attribute (NzChroma:PHYSICAL_CONTIGUOUS)
#pragma SDS data access_pattern(NzChroma:RANDOM)
#pragma SDS data zero_copy(NzChroma[0:2][0:PicWidthInMBs*2][0:FrameHeightInMbs*2])

#pragma SDS data mem_attribute (nalu_buf:PHYSICAL_CONTIGUOUS)
#pragma SDS data access_pattern(nalu_buf:RANDOM)
#pragma SDS data zero_copy(nalu_buf[0:MAXNALBUFFERSIZE])

#pragma SDS data mem_attribute (PIC:PHYSICAL_CONTIGUOUS)
#pragma SDS data access_pattern(PIC:RANDOM)
#pragma SDS data zero_copy(PIC[0:MAX_REFERENCE_PICTURES])

#pragma SDS data mem_attribute (PIC_Sluma:PHYSICAL_CONTIGUOUS)
#pragma SDS data access_pattern(PIC_Sluma:RANDOM)
#pragma SDS data zero_copy(PIC_Sluma[0:MAX_REFERENCE_PICTURES][0:PicWidthInSamplesL][0:FrameHeightInSampleL])

#pragma SDS data mem_attribute (PIC_SChroma_0:PHYSICAL_CONTIGUOUS)
#pragma SDS data access_pattern(PIC_SChroma_0:RANDOM)
#pragma SDS data zero_copy(PIC_SChroma_0[0:MAX_REFERENCE_PICTURES][0:PicWidthInSamplesC][0:FrameHeightInSampleC])

#pragma SDS data mem_attribute (PIC_SChroma_1:PHYSICAL_CONTIGUOUS)
#pragma SDS data access_pattern(PIC_SChroma_1:RANDOM)
#pragma SDS data zero_copy(PIC_SChroma_1[0:MAX_REFERENCE_PICTURES][0:PicWidthInSamplesC][0:FrameHeightInSampleC])
void process_chroma
(
 unsigned char CodedPatternChroma,
 unsigned char NzChroma[2][PicWidthInMBs*2][FrameHeightInMbs*2],
 unsigned char Imode[PicWidthInMBs][FrameHeightInMbs],
 int mbaddrx,
 int mbaddry,
 NALU_t* nalu,
 unsigned char *nalu_buf,
 unsigned long int *nalu_bit_offset,
 char qPc,
 char qPcm6,
 char temp1c,
 char temp2c,
 char temp3c,
 int coeffDCC_0[4][2],
 int coeffDCC_1[4][2],
 char refidx0[2][2],
 char refidx1[2][2],
 int mvd0[4][4][2],
 int mvd1[4][4][2],
 ImageParameters* img,
 unsigned char tmpImode,
 unsigned char predC_0[4][4][4],
 unsigned char predC_1[4][4][4],
 //StorablePicture PIC[MAX_REFERENCE_PICTURES],
 StorablePicture *PIC,
 unsigned char PIC_Sluma[MAX_REFERENCE_PICTURES][PicWidthInSamplesL][FrameHeightInSampleL],
 unsigned char PIC_SChroma_0[MAX_REFERENCE_PICTURES][PicWidthInSamplesC][FrameHeightInSampleC],
 unsigned char PIC_SChroma_1[MAX_REFERENCE_PICTURES][PicWidthInSamplesC][FrameHeightInSampleC]
 )

{
// Disable HLS directives on function parameters when the function is a top-module for SDSoC/HLS.
#if 0
//#pragma HLS ARRAY_PARTITION variable=predC_0 complete dim=2
//#pragma HLS ARRAY_PARTITION variable=predC_0 complete dim=3
//#pragma HLS ARRAY_PARTITION variable=predC_1 complete dim=2
//#pragma HLS ARRAY_PARTITION variable=predC_1 complete dim=3

//#pragma HLS ARRAY_PARTITION variable=coeffDCC_0 complete dim=1
//#pragma HLS ARRAY_PARTITION variable=coeffDCC_0 complete dim=2
//#pragma HLS ARRAY_PARTITION variable=coeffDCC_1 complete dim=1
//#pragma HLS ARRAY_PARTITION variable=coeffDCC_1 complete dim=2

//#pragma HLS ARRAY_PARTITION variable=mvd0 complete dim=1
//#pragma HLS ARRAY_PARTITION variable=mvd0 complete dim=2
//#pragma HLS ARRAY_PARTITION variable=mvd0 complete dim=3
//#pragma HLS ARRAY_PARTITION variable=mvd1 complete dim=1
//#pragma HLS ARRAY_PARTITION variable=mvd1 complete dim=2
//#pragma HLS ARRAY_PARTITION variable=mvd1 complete dim=3
#endif

  int nC = 0;
  int coeffACC_0[2][2][4][4] = { { { { 0 } } } };
  int coeffACC_1[2][2][4][4] = { { { { 0 } } } };
#pragma HLS ARRAY_PARTITION variable=coeffACC_0 complete dim=3
#pragma HLS ARRAY_PARTITION variable=coeffACC_0 complete dim=4
#pragma HLS ARRAY_PARTITION variable=coeffACC_1 complete dim=3
#pragma HLS ARRAY_PARTITION variable=coeffACC_1 complete dim=4

  int i = 0, j = 0;
  int rMbC_0[2][2][4][4] = { { { { 0 } } } };
  int rMbC_1[2][2][4][4] = { { { { 0 } } } };
#pragma HLS ARRAY_PARTITION variable=rMbC_0 complete dim=3
#pragma HLS ARRAY_PARTITION variable=rMbC_0 complete dim=4
#pragma HLS ARRAY_PARTITION variable=rMbC_1 complete dim=3
#pragma HLS ARRAY_PARTITION variable=rMbC_1 complete dim=4

  int mvdC0[2][2][2] = { { { 0 } } };
  int mvdC1[2][2][2] = { { { 0 } } };
#pragma HLS ARRAY_PARTITION variable=mvdC0 complete dim=1
#pragma HLS ARRAY_PARTITION variable=mvdC0 complete dim=2
#pragma HLS ARRAY_PARTITION variable=mvdC0 complete dim=3
#pragma HLS ARRAY_PARTITION variable=mvdC1 complete dim=1
#pragma HLS ARRAY_PARTITION variable=mvdC1 complete dim=2
#pragma HLS ARRAY_PARTITION variable=mvdC1 complete dim=3
  int tmpidx0 = 0;
  int tmpidx1 = 0;
  int x = 0, y = 0;
  //processing CR component of each sample
  //first channel
  for(y=0;y<2;y++)
  for(x=0;x<2;x++)
  {

    if(CodedPatternChroma&0x2)
    {
      nC=nc_Chroma(Imode,NzChroma[0],mbaddrx*2+x,mbaddry*2+y);
      NzChroma[0][mbaddrx*2+x][mbaddry*2+y]=residual_block_cavlc_16(coeffACC_0[x][y],nalu,nalu_buf, nalu_bit_offset,1,15,nC);
    }
    else
    {
      NzChroma[0][mbaddrx*2+x][mbaddry*2+y]=0;
      for(i=0;i<4;i++)
        #pragma HLS PIPELINE
        for(j=0;j<4;j++)
        {
          rMbC_0[x][y][i][j]=0;
          coeffACC_0[x][y][i][j]=0;
        }
    }
  }

  //second channel
  for(y=0;y<2;y++)
  for(x=0;x<2;x++)
  {

    if(CodedPatternChroma&0x2)
    {
      nC=nc_Chroma(Imode,NzChroma[1],mbaddrx*2+x,mbaddry*2+y);
      NzChroma[1][mbaddrx*2+x][mbaddry*2+y]=residual_block_cavlc_16(coeffACC_1[x][y],nalu,nalu_buf, nalu_bit_offset,1,15,nC);
    }
    else
    {
      NzChroma[1][mbaddrx*2+x][mbaddry*2+y]=0;
      for(i=0;i<4;i++)
        #pragma HLS PIPELINE
        for(j=0;j<4;j++)
        {
          rMbC_1[x][y][i][j]=0;
          coeffACC_1[x][y][i][j]=0;
        }
    }
  }


  for(y=0;y<2;y++)
  for(x=0;x<2;x++)
  {

    if(CodedPatternChroma&0x3)
    {
      scale_residual4x4_and_trans_inverse(qPc, qPcm6, temp1c, temp2c, temp3c, coeffACC_0[x][y],rMbC_0[x][y], coeffDCC_0[x][y],1);
      scale_residual4x4_and_trans_inverse(qPc, qPcm6, temp1c, temp2c, temp3c, coeffACC_1[x][y],rMbC_1[x][y], coeffDCC_1[x][y],1);
    }
    if(refidx0[x][y]>=0 && refidx1[x][y]>=0)
    {
      //LOOP_COPY:
	  for(i=0;i<2;i++)
        #pragma HLS PIPELINE
        for(j=0;j<2;j++)
        {
          mvdC0[i][j][0]=mvd0[x*2+i][y*2+j][0];
          mvdC1[i][j][0]=mvd1[x*2+i][y*2+j][0];
          mvdC0[i][j][1]=mvd0[x*2+i][y*2+j][1];
          mvdC1[i][j][1]=mvd1[x*2+i][y*2+j][1];
        }
      tmpidx0=img->list0[(unsigned)refidx0[x][y]];
      tmpidx1=img->list1[(unsigned)refidx1[x][y]];
    }
    else if(refidx0[x][y]>=0 && refidx1[x][y]<0)
    {
      //LOOP_COPY2:
	  for(i=0;i<2;i++)
        #pragma HLS PIPELINE
        for(j=0;j<2;j++)
        {
          mvdC0[i][j][0]=mvd0[x*2+i][y*2+j][0];
          mvdC0[i][j][1]=mvd0[x*2+i][y*2+j][1];
        }
      tmpidx0=img->list0[(unsigned)refidx0[x][y]];
    }
    else if(refidx0[x][y]<0 && refidx1[x][y]>=0)
    {
      //LOOP_COPY3:
	  for(i=0;i<2;i++)
        #pragma HLS PIPELINE
        for(j=0;j<2;j++)
        {
          mvdC0[i][j][0]=mvd1[x*2+i][y*2+j][0];
          mvdC0[i][j][1]=mvd1[x*2+i][y*2+j][1];
        }
      tmpidx0=img->list1[(unsigned)refidx1[x][y]];
    }

    if(tmpImode==INTRA4x4 || tmpImode== INTRA16x16)
    {
      write_Chroma(predC_0[x+y*2],rMbC_0[x][y],PIC_SChroma_0[img->mem_idx],(mbaddrx*2+x)*4,(mbaddry*2+y)*4,tmpImode==INTERSKIP );
      write_Chroma(predC_1[x+y*2],rMbC_1[x][y],PIC_SChroma_1[img->mem_idx],(mbaddrx*2+x)*4,(mbaddry*2+y)*4,tmpImode==INTERSKIP );
    }
    else if(refidx0[x][y]>=0 && refidx1[x][y]>=0)
    {
      inter_prediction_chroma_subblock_double(rMbC_0[x][y],mvdC0,mvdC1, PIC_SChroma_0[tmpidx0],PIC_SChroma_0[tmpidx1],PIC_SChroma_0[img->mem_idx],(mbaddrx*2+x)*4,(mbaddry*2+y)*4,(CodedPatternChroma&0x3) !=0);
      inter_prediction_chroma_subblock_double(rMbC_1[x][y],mvdC0,mvdC1, PIC_SChroma_1[tmpidx0],PIC_SChroma_1[tmpidx1],PIC_SChroma_1[img->mem_idx],(mbaddrx*2+x)*4,(mbaddry*2+y)*4,(CodedPatternChroma&0x3) !=0);
    }
    else
    {
      inter_prediction_chroma_subblock_single(rMbC_0[x][y],mvdC0,PIC_SChroma_0[tmpidx0],PIC_SChroma_0[img->mem_idx],(mbaddrx*2+x)*4,(mbaddry*2+y)*4,(CodedPatternChroma&0x3) !=0);
      inter_prediction_chroma_subblock_single(rMbC_1[x][y],mvdC0,PIC_SChroma_1[tmpidx0],PIC_SChroma_1[img->mem_idx],(mbaddrx*2+x)*4,(mbaddry*2+y)*4,(CodedPatternChroma&0x3) !=0);
    }

  }
}

void ProcessSlice
(
 NALU_t* nalu,
 unsigned char *nalu_buf, unsigned long int *nalu_bit_offset,
 StorablePicture PIC[MAX_REFERENCE_PICTURES],
 unsigned char PIC_Sluma[MAX_REFERENCE_PICTURES][PicWidthInSamplesL][FrameHeightInSampleL],
 unsigned char PIC_SChroma_0[MAX_REFERENCE_PICTURES][PicWidthInSamplesC][FrameHeightInSampleC],
 unsigned char PIC_SChroma_1[MAX_REFERENCE_PICTURES][PicWidthInSamplesC][FrameHeightInSampleC],
 StorablePictureInfo  PICINFO[MAX_REFERENCE_PICTURES],
 unsigned char Imode[PicWidthInMBs][FrameHeightInMbs],
 char IntraPredMode[PicWidthInMBs*4][FrameHeightInMbs*4],
 unsigned char NzLuma[PicWidthInMBs*4][FrameHeightInMbs*4],
 unsigned char NzChroma[2][PicWidthInMBs*2][FrameHeightInMbs*2],
 unsigned char constraint_intra_flag,
 slice_header_rbsp_t *SH,
 ImageParameters* img
 )
{
  int qPprev=img->sliceQPY;
  int type=SH->slice_type;

  //Tables
  const int qPCtable[22]={29,30,31,32,32,33,34,34,35,35,36,36,37,37,37,38,38,38,39,39,39,39};
  const int power2[6]={1,2,4,8,16,32};
  const int intratypecutoff[3]={5,23,0};

  //tempvariables
  unsigned char MbType=0;
  unsigned char MbSkipFlag=0;
  unsigned char PrevSkip=0;
  int MbSkipRun=0;
  unsigned char tmpImode=0;

  unsigned char nC=0;

  unsigned char tmpmbtp=0;
  //unsigned char avaimode;
  //int tmpidx0=0;
  //int tmpidx1=0;

  //tempsyntaxes
  unsigned char coded_block_pattern=0;
  unsigned char CodedPatternLuma=0;
  unsigned char CodedPatternChroma=0;
  unsigned char IntraChromaPredMode=0;
  unsigned char Intra16x16PredMode=0;
  int mb_qp_delta=0;

  //counter
  int i=0,j=0,k=0;
  int x=0,y=0;
  int mbaddrx=0,mbaddry=0;

  //inter pred info matrix
  char refCOL[2][2] = { { 0 } };
#pragma HLS ARRAY_PARTITION variable=refCOL complete dim=1
#pragma HLS ARRAY_PARTITION variable=refCOL complete dim=1

  int mvCOL[4][4][2] = { { { 0 } } };
#pragma HLS ARRAY_PARTITION variable=mvCOL complete dim=1
#pragma HLS ARRAY_PARTITION variable=mvCOL complete dim=2
#pragma HLS ARRAY_PARTITION variable=mvCOL complete dim=3

  char refidx0[2][2] = { { 0 } };
  char refidx1[2][2] = { { 0 } };
#pragma HLS ARRAY_PARTITION variable=refidx0 complete dim=1
#pragma HLS ARRAY_PARTITION variable=refidx0 complete dim=2
#pragma HLS ARRAY_PARTITION variable=refidx1 complete dim=1
#pragma HLS ARRAY_PARTITION variable=refidx1 complete dim=2

  int mvd0[4][4][2] = { { { 0 } } };
  int mvd1[4][4][2] = { { { 0 } } };
#pragma HLS ARRAY_PARTITION variable=mvd0 complete dim=1
#pragma HLS ARRAY_PARTITION variable=mvd0 complete dim=2
#pragma HLS ARRAY_PARTITION variable=mvd0 complete dim=3
#pragma HLS ARRAY_PARTITION variable=mvd1 complete dim=1
#pragma HLS ARRAY_PARTITION variable=mvd1 complete dim=2
#pragma HLS ARRAY_PARTITION variable=mvd1 complete dim=3

  //int mvdC0[2][2][2];
  //int mvdC1[2][2][2];

#pragma HLS ARRAY_PARTITION variable=mvdC0 complete dim=3
#pragma HLS ARRAY_PARTITION variable=mvdC0 complete dim=2
#pragma HLS ARRAY_PARTITION variable=mvdC0 complete dim=1
#pragma HLS ARRAY_PARTITION variable=mvdC1 complete dim=3
#pragma HLS ARRAY_PARTITION variable=mvdC1 complete dim=2
#pragma HLS ARRAY_PARTITION variable=mvdC1 complete dim=1

  //intra pred info array
  char intra4x4predmode[16] = { 0 };

  //prediction matrix
  unsigned char predL[16][4][4] = { { { 0 } } };
  unsigned char predC_0[4][4][4] = { { { 0 } } };
  unsigned char predC_1[4][4][4] = { { { 0 } }  };
#pragma HLS ARRAY_PARTITION variable=predL complete dim=2
#pragma HLS ARRAY_PARTITION variable=predL complete dim=3

#pragma HLS ARRAY_PARTITION variable=predC_0 complete dim=2
#pragma HLS ARRAY_PARTITION variable=predC_0 complete dim=3
#pragma HLS ARRAY_PARTITION variable=predC_1 complete dim=2
#pragma HLS ARRAY_PARTITION variable=predC_1 complete dim=3

  //quant temp

  char qPm6 = 0, qPi = 0, qPy = 0, qPc = 0, qPcm6 = 0;
  char temp1l = 0, temp2l = 0, temp3l = 0;
  char temp1c = 0, temp2c = 0, temp3c = 0;
  char scale1 = 0, scale2 = 0, scale3 = 0;
  //residual matrix
  int coeffDCL[4][4] = { { 0 } };
#pragma HLS ARRAY_PARTITION variable=coeffDCL complete dim=1
#pragma HLS ARRAY_PARTITION variable=coeffDCL complete dim=2


  int coeffDCC_0[4][2] = { { 0 } };
  int coeffDCC_1[4][2] = { { 0 } };
#pragma HLS ARRAY_PARTITION variable=coeffDCC_0 complete dim=1
#pragma HLS ARRAY_PARTITION variable=coeffDCC_0 complete dim=2
#pragma HLS ARRAY_PARTITION variable=coeffDCC_1 complete dim=1
#pragma HLS ARRAY_PARTITION variable=coeffDCC_1 complete dim=2

  //int rMbL[4][4];
  //int rMbC[4][4];
//#pragma HLS ARRAY_PARTITION variable=rMbL complete dim=1
//#pragma HLS ARRAY_PARTITION variable=rMbL complete dim=2
//#pragma HLS ARRAY_PARTITION variable=rMbC complete dim=1
//#pragma HLS ARRAY_PARTITION variable=rMbC complete dim=2

  //inter mediate residual
  for(mbaddry=0;mbaddry<FrameHeightInMbs;mbaddry++)
    //LOOP_MACROBLOCK:
	for(mbaddrx=0;mbaddrx<PicWidthInMBs;mbaddrx++)
    {
#if _N_HLS_
      fprintf(trace_bit,"\nMbaddr %d\n",mbaddry*PicWidthInMBs+mbaddrx);
      fprintf(prediction_test,"\nMbaddr %d\n",mbaddry*PicWidthInMBs+mbaddrx);
      fprintf(construction_test,"\nMbaddr %d\n",mbaddry*PicWidthInMBs+mbaddrx);
#endif
      if(type!= SLICE_TYPE_I)
      {
        if(MbSkipRun==0 && PrevSkip==0)
        {
          MbSkipRun=u_e(nalu, nalu_buf, nalu_bit_offset);
#if _N_HLS_
          fprintf(trace_bit,"\n mbskip_run %d\n", MbSkipRun);
#endif
          if(MbSkipRun==0)
          {
            MbSkipFlag=0;
            PrevSkip=0;
          }
          else
          {
            MbSkipFlag=1;
            PrevSkip=1;
            MbSkipRun--;
          }
        }
        else if(MbSkipRun>0)
        {
          MbSkipFlag=1;
          MbSkipRun--;
        }
        else
        {
          MbSkipFlag=0;
          PrevSkip=0;
        }
      }

      // following decide the skip mode
      if(MbSkipFlag==0)
      {
        tmpmbtp=MbType=u_e(nalu,nalu_buf, nalu_bit_offset);
#if _N_HLS_
        fprintf(trace_bit,"mbtype %d\n",MbType);
#endif // _N_HLS_
        if(MbType>=intratypecutoff[type])
        {
          MbType=MbType-intratypecutoff[type];
          tmpImode=Imode[mbaddrx][mbaddry]=(MbType==0 || MbType==25)?MbType:1;
        }
        else
        {
          MbType=MbType+(type?7:1);
          tmpImode=Imode[mbaddrx][mbaddry]=INTER4x4;
        }
      }
      else
      {
        MbType=(type?6:0);
        tmpImode=Imode[mbaddrx][mbaddry]=INTERSKIP;
        CodedPatternChroma=0;
        CodedPatternLuma=0;
      }

      // following function will process the prediction information
      if(tmpImode==INTER4x4||tmpImode==INTERSKIP)
      {
        // this part will read and calculate the inter prediction information
        //preparing for mvcol
        if(type==SLICE_TYPE_B)
        {
          for(i=0;i<4;i++)
            for(j=0;j<4;j++)
            {

              if( PICINFO[img->list1[0]].refIdxL0[(mbaddrx*4+i)/2][(mbaddry*4+j)/2]>=0)
              {
                refCOL[i/2][j/2]=PICINFO[img->list1[0]].refIdxL0[(mbaddrx*4+i)/2][(mbaddry*4+j)/2];
                mvCOL[i][j][0]=PICINFO[img->list1[0]].mvd_l0[mbaddrx*4+i][mbaddry*4+j][0];
                mvCOL[i][j][1]=PICINFO[img->list1[0]].mvd_l0[mbaddrx*4+i][mbaddry*4+j][1];
              }
              else
              {
                refCOL[i/2][j/2]=PICINFO[img->list1[0]].refIdxL1[(mbaddrx*4+i)/2][(mbaddry*4+j)/2];
                mvCOL[i][j][0]=PICINFO[img->list1[0]].mvd_l1[mbaddrx*4+i][mbaddry*4+j][0];
                mvCOL[i][j][1]=PICINFO[img->list1[0]].mvd_l1[mbaddrx*4+i][mbaddry*4+j][1];
              }
            }

        }
        // Set Intra prediction info
        //LOOP_ZERO_INTRAMODED:
		for(i=0;i<4;i++)
        {
          #pragma HLS UNROLL
          IntraPredMode[mbaddrx*4+3][mbaddry*4+i]=2;
          IntraPredMode[mbaddrx*4+i][mbaddry*4+3]=2;
        }


        processinterMbType(
         tmpmbtp,
         type,
         nalu,
         nalu_buf, nalu_bit_offset,
         SH->num_ref_idx_l1_active_minus1,
         SH->num_ref_idx_l0_active_minus1,
         refidx0,
         refidx1,
         mvd0,
         mvd1,
         PICINFO[img->mem_idx].refIdxL0,
         PICINFO[img->mem_idx].refIdxL1,
         PICINFO[img->mem_idx].mvd_l0,
         PICINFO[img->mem_idx].mvd_l1,
         img->list0,
         img->list1,
         mbaddrx+mbaddry*PicWidthInMBs,
         MbSkipFlag,
         refCOL,
         mvCOL,
         MbType);
      }
      else if(tmpImode==INTRA16x16 ||tmpImode==INTRA4x4 )
      {
        // this part will read and calculate the intra prediction information
        if(tmpImode==INTRA4x4)
        {
          IntraInfo(nalu,nalu_buf, nalu_bit_offset,IntraPredMode,PICINFO[img->mem_idx].refIdxL0,PICINFO[img->mem_idx].refIdxL1,intra4x4predmode, constraint_intra_flag,mbaddrx*4,mbaddry*4);
        }
        else
        {
          for(i=0;i<4;i++)
          {
            #pragma HLS UNROLL
            IntraPredMode[mbaddrx*4+3][mbaddry*4+i]=2;
            IntraPredMode[mbaddrx*4+i][mbaddry*4+3]=2;
          }
        }
        Intra16x16PredMode=(MbType-1)%4;
        IntraChromaPredMode=u_e(nalu,nalu_buf, nalu_bit_offset);
#if _N_HLS_
        fprintf(trace_bit,"%s %*d\n","intra_chroma_pred_mode",50-strlen("intra_chroma_pred_mode"),IntraChromaPredMode);
#endif // _N_HLS_
      }
      if(tmpImode!=IPCM16x16)
      {
        if(tmpImode!=INTERSKIP)
        {
          if(tmpImode!=INTRA16x16)
          {
            coded_block_pattern=m_e( (tmpImode!=INTRA4x4), nalu,nalu_buf, nalu_bit_offset);
#if _N_HLS_
            fprintf(trace_bit,"%s %*d\n","coded_block_pattern",50-strlen("coded_block_pattern"),coded_block_pattern);
#endif // _N_HLS_
            CodedPatternLuma=coded_block_pattern%16;
            CodedPatternChroma=coded_block_pattern/16;

          }
          else
          {
            CodedPatternChroma=(MbType-1)/4%3;
            CodedPatternLuma=MbType>12?15:0;
          }

          if(CodedPatternChroma>0 || CodedPatternLuma>0 || tmpImode==INTRA16x16)
          {
            mb_qp_delta=s_e(nalu,nalu_buf, nalu_bit_offset);
#if _N_HLS_
            fprintf(trace_bit,"%s %*d\n","mb_qp_delta",50-strlen("mb_qp_delta"),mb_qp_delta);
#endif // _N_HLS_
            qPprev+=mb_qp_delta;
            qPy=qPprev;
            qPm6=qPy%6;
            temp1l=qPy/6-4;
            temp2l=4-qPy/6;

            if(temp1l<0)
              temp3l=power2[3-qPy/6];

            scale1=qPy/6-6;
            scale2=6-qPy/6;

            if(scale1<0)
              scale3=power2[5-qPy/6];


            qPi=Clip3(0,51,qPy+img->chroma_offset);

            if(qPi<30)
              qPc=qPi;
            else
              qPc=qPCtable[qPi-30];

            qPcm6=qPc%6;

            temp1c=qPc/6-4;
            temp2c=4-qPc/6;
            temp3c=1<<(3-qPc/6);
          }
        }

        if(tmpImode==INTRA16x16)
        {
          //processing INTRA16x16 DC
          nC=nc_Luma(Imode,NzLuma,mbaddrx*4,mbaddry*4);
          residual_block_cavlc_16(coeffDCL,nalu,nalu_buf, nalu_bit_offset,0,15,nC);
          //Scaling
          scale_and_inv_trans_Intra16x16DC(qPy,coeffDCL,qPm6,scale1,scale2,scale3);
          predict_intra16x16_luma_NonField(predL,PIC_Sluma[img->mem_idx],Intra16x16PredMode,(mbaddrx>0)*2+(mbaddry>0),mbaddrx*16,mbaddry*16);
        }

        //LOOP_LUMA_SUBBLOCK:
		for(k=0;k<16;k++)
        {
          x=KTOX(k);
          y=KTOY(k);

          //processing luma component of each block
          process_luma(
           x,
           y,
           k,
           mbaddrx,
           mbaddry,
           CodedPatternLuma,
           nalu,
           nalu_buf,
		   nalu_bit_offset,
           tmpImode,
           coeffDCL[x][y],
           PIC,
           PIC_Sluma,
           PIC_SChroma_0,
           PIC_SChroma_1,
           Imode,
           IntraPredMode,
           NzLuma,
           img,
           predL[k],
           qPm6,
           qPy,
           temp1l,
           temp2l,
           temp3l,
           refidx0[x/2][y/2],
           refidx1[x/2][y/2],
           mvd0[x][y],
           mvd1[x][y],
           intra4x4predmode[k]);
        }
        if(CodedPatternChroma&0x3)
        {
          residual_block_cavlc_4(coeffDCC_0, nalu, nalu_buf, nalu_bit_offset, 0,3);
          scale_and_inv_trans_chroma2x2(coeffDCC_0,qPc, qPc%6);
          residual_block_cavlc_4(coeffDCC_1, nalu, nalu_buf, nalu_bit_offset, 0,3);
          scale_and_inv_trans_chroma2x2(coeffDCC_1,qPc, qPc%6);
        }

        if(tmpImode==INTRA16x16 || tmpImode==INTRA4x4)
        {
          prediction_Chroma(predC_0,PIC_SChroma_0[img->mem_idx],(mbaddrx>0)*2+(mbaddry>0),mbaddrx*8,mbaddry*8,IntraChromaPredMode);
          prediction_Chroma(predC_1,PIC_SChroma_1[img->mem_idx],(mbaddrx>0)*2+(mbaddry>0),mbaddrx*8,mbaddry*8,IntraChromaPredMode);
        }
        //processing CR component

        process_chroma(
          CodedPatternChroma,
          NzChroma,
          Imode,
          mbaddrx,
          mbaddry,
          nalu,
          nalu_buf, nalu_bit_offset,
          qPc,
          qPcm6,
          temp1c,
          temp2c,
          temp3c,
          coeffDCC_0,
          coeffDCC_1,
          refidx0,
          refidx1,
          mvd0,
          mvd1,
          img,
          tmpImode,
          predC_0,
          predC_1,
          PIC,
          PIC_Sluma,
          PIC_SChroma_0,
          PIC_SChroma_1);
      }
    }
}






