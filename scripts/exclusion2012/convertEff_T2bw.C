#include <algorithm>
#include <iostream>
#include <vector>
#include <math.h>
#include <fstream>
#include <sstream>
#include <iomanip>

#include "TCanvas.h"
#include "TLegend.h"
#include "TChain.h"
#include "TDirectory.h"
#include "TFile.h"
#include "TROOT.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TMath.h"
#include "TPad.h"
#include "TCut.h"
#include "TProfile.h"
#include "THStack.h"
#include "TLatex.h"
#include "TGraphErrors.h"
#include "TStyle.h"
#include "TLine.h"
#include "TMath.h"
#include "TPaletteAxis.h"

using namespace std;

//------------------------------------
// remove points with mLSP > y
//------------------------------------

void blankHist( TH2F* h , float y ){

  for(int ibin = 1 ; ibin <= h->GetXaxis()->GetNbins() ; ibin++ ){
    for(int jbin = 1 ; jbin <= h->GetYaxis()->GetNbins() ; jbin++ ){
      if( h->GetYaxis()->GetBinCenter(jbin) > y ) h->SetBinContent(ibin,jbin,0);
    }
  }

}

void truncateHistAtDiagonal(TH2F* h,int cutval){

  for( int ibin = 1 ; ibin <= h->GetXaxis()->GetNbins() ; ibin++ ){
    for( int jbin = 1 ; jbin <= h->GetYaxis()->GetNbins() ; jbin++ ){
      float mstop = h->GetXaxis()->GetBinCenter(ibin);
      float mlsp  = h->GetYaxis()->GetBinCenter(jbin);
      float dm    = mstop - mlsp;

      if( dm <= cutval ) h->SetBinContent(ibin,jbin,0);
    }
  }

}

void convertEff_T2bw(){

  TFile* fin = TFile::Open("rootfiles/T2bw_MG_x50T2BW_SS_histos.root");

  TFile* fin_combined = TFile::Open("rootfiles/T2bw_MG_x50_combinePlotsT2BW_SS_nmin20.root");

  TH2F* h[8];

  TCanvas *c1 = new TCanvas("c1","c1",1800,800);
  c1->Divide(4,2);

  TFile* fout = TFile::Open("bottomchargino_x50_cutbased_efficiencies.root","RECREATE");
  gROOT->cd();

  char* labels[8] = {"LM100","LM150","LM200","LM250","HM100","HM150","HM200","HM250"};

  for( int i = 0 ; i < 8 ; ++i ){

    h[i]  = (TH2F*) fin->Get(Form("heff_%i",i));
    h[i]->SetName(Form("efficiency_%s",labels[i]));
    h[i]->SetTitle(Form("efficiency_%s",labels[i]));

    truncateHistAtDiagonal(h[i],175);
    blankHist(h[i],300);

    h[i]->Scale(1.0/100.0);

    h[i]->GetXaxis()->SetRangeUser(200,800);
    h[i]->GetYaxis()->SetRangeUser(0,450);
    h[i]->SetMinimum(1e-6);
    h[i]->SetMaximum(1.0);

    h[i]->GetXaxis()->SetTitle("m_{#tilde{t}} [GeV]");
    h[i]->GetYaxis()->SetTitle("m_{#tilde{#chi}_{1}^{0}} [GeV]");
    h[i]->GetZaxis()->SetTitle("efficiency #times acceptance");

    c1->cd(i+1);
    gPad->SetLogz(1);
    gPad->SetRightMargin(0.2);
    gPad->SetTopMargin(0.07);

    gPad->Update();
    TPaletteAxis *palette = (TPaletteAxis*)h[i]->GetListOfFunctions()->FindObject("palette");
    delete palette;

    h[i]->Draw("colz");

    h[i]->GetZaxis()->SetTitle("efficiency #times acceptance");

    TLatex *text = new TLatex();
    text->SetNDC();
    text->SetTextSize(0.04);
    text->DrawLatex(0.17,0.96,"CMS Simulation            #sqrt{s} = 8 TeV, #scale[0.6]{#int}Ldt = 19.5 fb^{-1}");

    text->SetTextSize(0.05);
    text->DrawLatex(0.20,0.83,"pp #rightarrow #tilde{t} #tilde{t}*, #tilde{t} #rightarrow b #tilde{#chi}_{1}^{+} x=0.5");
    text->SetTextSize(0.07);
    text->DrawLatex(0.20,0.75,labels[i]);

    fout->cd();
    h[i]->Write();

  }

  int bin = h[0]->FindBin(225,0);
  cout << "225/0 LM100 " << h[0]->GetBinContent(bin) << " " << h[0]->GetBinContent(bin)*19500.0*9.9 << endl;

  bin = h[0]->FindBin(225,25);
  cout << "225/25 LM100 " << h[0]->GetBinContent(bin) << " " << h[0]->GetBinContent(bin)*19500.0*9.9 << endl;

  bin = h[0]->FindBin(650,0);
  cout << "650/0 HM250 " << h[7]->GetBinContent(bin) << " " << h[7]->GetBinContent(bin)*19500.0*0.0139566 << endl;

  bin = h[0]->FindBin(675,0);
  cout << "675/0 HM250 " << h[7]->GetBinContent(bin) << " " << h[7]->GetBinContent(bin)*19500.0*0.0106123 << endl;

  bin = h[0]->FindBin(700,0);
  cout << "700/0 HM250 " << h[7]->GetBinContent(bin) << " " << h[7]->GetBinContent(bin)*19500.0*0.0081141 << endl;

  c1->Print("bottomchargino_x50_cutbased_efficiencies.C");
  c1->Print("plots/bottomchargino_x50_cutbased_efficiencies.pdf");
  c1->Print("plots/bottomchargino_x50_cutbased_efficiencies.png");

  TH2F* hmap = (TH2F*) fin_combined->Get("hbest");

  TCanvas *c2 = new TCanvas();
  hmap->Draw("colz");
  hmap->Draw("sametext");

  hmap->SetName("best_signal_region");
  hmap->SetTitle("best_signal_region");

  hmap->Write();
  fout->Close();
}
