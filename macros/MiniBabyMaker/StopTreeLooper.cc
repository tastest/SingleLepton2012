//#include "../../CORE/jetSmearingTools.h"
#include "../../CORE/Thrust.h"
#include "../../CORE/EventShape.h"

#include "StopTreeLooper.h"

//#include "../Core/STOPT.h"
#include "../Core/stopUtils.h"
#include "../Plotting/PlotUtilities.h"
#include "../Core/MT2Utility.h"
#include "../Core/mt2bl_bisect.h"
#include "../Core/mt2w_bisect.h"
#include "../Core/PartonCombinatorics.h"

#include "TROOT.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TChainElement.h"
#include "TFile.h"
#include "TMath.h"
#include "TChain.h"
#include "Riostream.h"
#include "TFitter.h"
#include "TRandom3.h"

#include <algorithm>
#include <utility>
#include <map>
#include <set>
#include <list>

using namespace Stop;

std::set<DorkyEventIdentifier> already_seen; 
std::set<DorkyEventIdentifier> events_lasercalib; 
std::set<DorkyEventIdentifier> events_hcallasercalib; 

StopTreeLooper::StopTreeLooper()
{
    m_outfilename_ = "histos.root";
    // t1metphicorr = -9999.;
    // t1metphicorrphi = -9999.;
    // t1metphicorrmt = -9999.;
    // min_mtpeak = -9999.;
    // max_mtpeak = -9999.; 
}

StopTreeLooper::~StopTreeLooper()
{
}

void StopTreeLooper::setOutFileName(string filename)
{
    m_outfilename_ = filename;

}

void StopTreeLooper::loop(TChain *chain, TString name)
{
    TRandom3 aleatorio;

    printf("[StopTreeLooper::loop] %s\n", name.Data());

    load_badlaserevents("../Core/badlaser_events.txt", events_lasercalib);
    load_badlaserevents("../Core/badhcallaser_events.txt", events_hcallasercalib);

    //---------------------------------
    // check for valid chain
    //---------------------------------

    TObjArray *listOfFiles = chain->GetListOfFiles();
    TIter fileIter(listOfFiles);
    if (listOfFiles->GetEntries() == 0) {
        cout << "[StopTreeLooper::loop] no files in chain" << endl;
        return;
    }

    //---------------------------------
    // set up histograms
    //---------------------------------

    gROOT->cd();

    makeTree(name.Data(), chain);

    TH2F *h_nsig, *h_nsig25, *h_nsig75 ;

    if( name.Contains("T2") ){
        char* h_nsig_filename = "";

        if( name.Contains("T2tt") )
            h_nsig_filename = "/nfs-3/userdata/stop/cms2V05-03-18_stoplooperV00-02-07/crabT2tt_3/myMassDB_T2tt.root";

        if( name.Contains("T2bw") )
            h_nsig_filename = "/nfs-3/userdata/stop/cms2V05-03-18_stoplooperV00-02-07/crabT2bw_3/myMassDB_T2bw_fine.root";

        cout << "[StopTreeLooper::loop] opening mass TH2 file " << h_nsig_filename << endl;

        TFile *f_nsig = TFile::Open(h_nsig_filename);

        assert(f_nsig);

        h_nsig = (TH2F*) f_nsig->Get("masses");

        if( name.Contains("T2bw") ){
            h_nsig25 = (TH2F*) f_nsig->Get("masses25");
            h_nsig75 = (TH2F*) f_nsig->Get("masses75");
        }


    }

    TMVA::Reader* reader[5];
    if ( __apply_mva ) {
        for (int i=1; i <= 4 ; i++){
            reader[i] = new TMVA::Reader( "!Color:!Silent" );
            reader[i]->AddVariable("mini_met", &met_);
            reader[i]->AddVariable("mini_chi2minprob", &chi2_);
            reader[i]->AddVariable("mini_mt2wmin", &mt2w_);
            reader[i]->AddVariable("mini_htssm/(mini_htosm+mini_htssm)", &htratiom_);
            reader[i]->AddVariable("mini_dphimjmin", &dphimjmin_);
            //		  reader[i]->AddVariable("mini_pt_b", &pt_b_);
            //		  reader[i]->AddVariable("mini_lep1pt", &lep1pt_);
            //		  reader[i]->AddVariable("mini_nb", &nb_);

            TString dir, prefix;
            dir    = "/home/users/magania/stop/SingleLepton2012/MVA/weights/";
            prefix = "classification_T2tt_";
            prefix += i;
            prefix += "_BDT";

            TString weightfile = dir + prefix + TString(".weights.xml");
            reader[i]->BookMVA( "BDT" , weightfile );
        }
    }

    // TFile* vtx_file = TFile::Open("vtxreweight/vtxreweight_Summer12_DR53X-PU_S10_9p7ifb_Zselection.root");
    // if( vtx_file == 0 ){
    //   cout << "vtxreweight error, couldn't open vtx file. Quitting!"<< endl;
    //   exit(0);
    // }

    // TH1F* h_vtx_wgt = (TH1F*)vtx_file->Get("hratio");
    // h_vtx_wgt->SetName("h_vtx_wgt");

    //
    // file loop
    //

    unsigned int nEventsPass=0;
    unsigned int nEventsChain=0;
    unsigned int nEvents = chain->GetEntries();
    nEventsChain = nEvents;
    ULong64_t nEventsTotal = 0;
    //  int i_permille_old = 0;

    bool isData = name.Contains("data") ? true : false;

    cout << "[StopTreeLooper::loop] running over chain with total entries " << nEvents << endl;

    while (TChainElement *currentFile = (TChainElement*)fileIter.Next()) {

        //---------------------------------
        // load the stop baby tree
        //---------------------------------
        TFile *file = new TFile( currentFile->GetTitle() );
        TTree *tree = (TTree*)file->Get("t");
        stopt.Init(tree);

        tree->CopyAddresses(outTree_);

        //---------------------------------
        // event loop
        //---------------------------------

        ULong64_t nEvents = tree->GetEntries();
        //nEvents = 1000;

        for(ULong64_t event = 0; event < nEvents; ++event) {
            stopt.GetEntry(event);
            tree->GetEntry(event);

            //---------------------------------
            // increment counters
            //---------------------------------

            ++nEventsTotal;
            if (nEventsTotal%10000==0) {
                ULong64_t i_permille = (int)floor(1000 * nEventsTotal / float(nEventsChain));
                //if (i_permille != i_permille_old) {//this prints too often!
                // xterm magic from L. Vacavant and A. Cerri
                if (isatty(1)) {
                    printf("\015\033[32m ---> \033[1m\033[31m%4.1f%%"
                            "\033[0m\033[32m <---\033[0m\015", i_permille/10.);
                    fflush(stdout);
                }
                //	i_permille_old = i_permille;
            }

            //---------------------
            // skip duplicates
            //---------------------
            if( isData ) {
                DorkyEventIdentifier id = {stopt.run(), stopt.event(), stopt.lumi() };
                if (is_duplicate(id, already_seen) ){
                    continue;
                }
                if (is_badLaserEvent(id,events_lasercalib) ){
                    //std::cout<< "Removed bad laser calibration event:" << run << "   " << event<<"\n";
                    continue;
                }
                if (is_badLaserEvent(id,events_hcallasercalib) ){
                    //std::cout<< "Removed bad hcal laser calibration event:" << run << "   " << event<<"\n";
                    continue;
                }
            }

            //------------------------------------------ 
            // event weight
            //------------------------------------------ 
            float evtweight = isData ? 1. : ( stopt.weight() * 19.5 * stopt.nvtxweight() * stopt.mgcor() );

            if( name.Contains("T2tt") ) {
                int bin = h_nsig->FindBin(stopt.mg(),stopt.ml());
                float nevents = h_nsig->GetBinContent(bin);
                evtweight =  stopt.xsecsusy() * 1000.0 / nevents; 

                //cout << "mg " << tree->mg_ << " ml " << tree->ml_ << " bin " << bin << " nevents " << nevents << " xsec " << tree->xsecsusy_ << " weight " << evtweight << endl;
            }

            if( name.Contains("T2bw") ) {
                float nevents = 0;
                if ( stopt.x() == 0.25 ){
                    int bin = h_nsig25->FindBin(stopt.mg(),stopt.ml());
                    nevents = h_nsig25->GetBinContent(bin);
                } else if ( stopt.x() == 0.50 ){
                    int bin = h_nsig->FindBin(stopt.mg(),stopt.ml());
                    nevents = h_nsig->GetBinContent(bin);
                } else if ( stopt.x() == 0.75 ){
                    int bin = h_nsig75->FindBin(stopt.mg(),stopt.ml());
                    nevents = h_nsig75->GetBinContent(bin);
                }

                assert ( nevents > 0 );

                evtweight =  stopt.xsecsusy() * 1000.0 / nevents; 

                //	cout << "mg " << stopt.mg() << " ml " << stopt.ml() << " x " << stopt.x() << " nevents " << nevents << " xsec " << stopt.xsecsusy() << " weight " << evtweight << endl;
            }

            float trigweight   = isData ? 1. : getsltrigweight(stopt.id1(), stopt.lep1().Pt(), stopt.lep1().Eta());
            float trigweightdl = isData ? 1. : getdltrigweight(stopt.id1(), stopt.id2());

            //------------------------------------------ 
            // selection criteria
            //------------------------------------------ 

            if ( !passEvtSelection(name) ) continue; // >=1 lepton, rho cut, MET filters, veto 2 nearby leptons
            if ( getNJets() < 4          ) continue; // >=3 jets
            if ( stopt.t1metphicorr() < 100.0    ) continue; // MET > 100 GeV

            //------------------------------------------ 
            // get list of candidates
            //------------------------------------------ 
            float met = stopt.t1metphicorr();
            float metphi = stopt.t1metphicorrphi();

            vector<LorentzVector> myJets;
            vector<float> myJetsTag;
            vector<float> myJetsSigma;
            for (unsigned int i =0; i<stopt.pfjets().size(); i++){
                LorentzVector jet = stopt.pfjets().at(i);
                if ( jet.Pt() < JET_PT ) continue;
                if ( fabs(jet.eta()) > JET_ETA ) continue;

                myJets.push_back(jet);
                myJetsTag.push_back(stopt.pfjets_csv().at(i));
                myJetsSigma.push_back(stopt.pfjets_sigma().at(i));
            }

            double x_mt2w = calculateMT2w(myJets, myJetsTag, stopt.lep1(), met, metphi);
            double x_chi2 = calculateChi2SNT(myJets, myJetsSigma, myJetsTag);

            //------------------------------------------ 
            // event shapes
            //------------------------------------------ 

            std::vector<LorentzVector>  myPfJets = stopt.pfjets();
            std::vector<LorentzVector>  jetVector;
            std::vector<LorentzVector>  jetLeptonVector;
            std::vector<LorentzVector>  jetLeptonMetVector;

            jetVector.clear();
            jetLeptonVector.clear();
            jetLeptonMetVector.clear();

            float HT_SSL=0;
            float HT_OSL=0;

            float HT_SSM=0;
            float HT_OSM=0;


            LorentzVector lep1_p4( stopt.lep1().px() , stopt.lep1().py() , 0 , stopt.lep1().E() ); 

            LorentzVector met_p4( stopt.t1metphicorr() * cos( stopt.t1metphicorr()) , 
                    stopt.t1metphicorr() * sin( stopt.t1metphicorr()) , 
                    0,
                    stopt.t1metphicorr()
                    );

            jetLeptonVector.push_back(lep1_p4);

            jetLeptonMetVector.push_back(lep1_p4);
            jetLeptonMetVector.push_back(met_p4);

            for ( unsigned int i=0; i<myPfJets.size() ; i++) {

                if( myPfJets.at(i).pt()<30 )          continue;
                if( fabs(myPfJets.at(i).eta())>2.4 )  continue;

                LorentzVector jet_p4( myPfJets.at(i).px() , myPfJets.at(i).py() , 0 , myPfJets.at(i).E() );

                // jetVector.push_back(myPfJets->at(i));
                // jetLeptonVector.push_back(myPfJets->at(i));
                // jetLeptonMetVector.push_back(myPfJets->at(i));

                jetVector.push_back(jet_p4);
                jetLeptonVector.push_back(jet_p4);
                jetLeptonMetVector.push_back(jet_p4);

                float dPhiL = getdphi(stopt.lep1().Phi(), myPfJets.at(i).phi() );
                float dPhiM = getdphi(stopt.t1metphicorrphi(), myPfJets.at(i).phi() );

                if(dPhiL<(3.14/2))  HT_SSL=HT_SSL+myPfJets.at(i).pt();
                if(dPhiL>=(3.14/2)) HT_OSL=HT_OSL+myPfJets.at(i).pt();

                if(dPhiM<(3.14/2))  HT_SSM=HT_SSM+myPfJets.at(i).pt();
                if(dPhiM>=(3.14/2)) HT_OSM=HT_OSM+myPfJets.at(i).pt();

            }

            // from jets only
            Thrust thrust(jetVector);
            EventShape eventshape(jetVector);

            // from jets and lepton
            Thrust thrustl(jetLeptonVector);
            EventShape eventshapel(jetLeptonVector);

            // from jets and lepton and MET
            Thrust thrustlm(jetLeptonMetVector);
            EventShape eventshapelm(jetLeptonMetVector);

            //------------------------------------------ 
            // datasets bit
            //------------------------------------------ 

            bool dataset_1l=false;

            if((isData) && name.Contains("muo") && (abs(stopt.id1()) == 13 ))  dataset_1l=true;
            if((isData) && name.Contains("ele") && (abs(stopt.id1()) == 11 ))  dataset_1l=true;

            if(!isData) dataset_1l=true;

            bool dataset_CR4=false;

            if((isData) && name.Contains("dimu") && (abs(stopt.id1()) == 13 ) && (abs(stopt.id2())==13) && (fabs( stopt.dilmass() - 91.) > 15.)) dataset_CR4=true;
            if((isData) && name.Contains("diel") && (abs(stopt.id1()) == 11 ) && (abs(stopt.id2())==11) && (fabs( stopt.dilmass() - 91.) > 15.)) dataset_CR4=true;
            if((isData) && name.Contains("mueg") && abs(stopt.id1()) != abs(stopt.id2())) dataset_CR4=true;

            if(!isData) dataset_CR4=true;

            //------------------------------------------ 
            // variables to add to baby
            //------------------------------------------ 

            initBaby(); // set all branches to -1

            vector<int> indexBJets=getBJetIndex(0.679,-1,-1);
            if(indexBJets.size()>0) pt_b_ = myPfJets.at(indexBJets.at(0)).pt();

            int J1Index=leadingJetIndex( stopt.pfjets(), -1, -1);
            int J2Index=leadingJetIndex( stopt.pfjets(), J1Index, -1);
            pt_J1_ = myPfJets.at(J1Index).pt();
            pt_J2_ = myPfJets.at(J2Index).pt();

            // which selections are passed
            sig_        = ( dataset_1l && passOneLeptonSelection(isData) && indexBJets.size()>=1 )    ? 1 : 0; // pass signal region preselection
            cr1_        = ( dataset_1l && passOneLeptonSelection(isData) && indexBJets.size()==0 )    ? 1 : 0; // pass CR1 (b-veto) control region preselection
            cr4_        = ( dataset_CR4 && passDileptonSelection(isData) && indexBJets.size()==1)     ? 1 : 0; // pass CR4 (dilepton) control region preselection
            cr5_        = ( dataset_1l && passLepPlusIsoTrkSelection(isData) && indexBJets.size()==1) ? 1 : 0; // pass CR1 (lepton+isotrack) control region preselection
            t2ttLM_     = pass_T2tt_LM(isData);
            t2ttHM_     = pass_T2tt_HM(isData);

            // kinematic variables
            met_        = stopt.t1metphicorr();       // MET (type1, MET-phi corrections)

            mt2w_ = x_mt2w;
            mt2w_ = x_chi2;

            // weights
            weight_     = evtweight;                    // event weight
            sltrigeff_  = trigweight;                   // trigger weight (single lepton)
            dltrigeff_  = trigweightdl;                 // trigger weight (dilepton)

            // hadronic variables 
            nb_         = indexBJets.size();            // nbjets (pT > 30, CSVM)
            njets_      = getNJets();             // njets (pT > 30, eta < 2.5)

            // lepton variables
            passisotrk_   = passIsoTrkVeto() ? 1 : 0; // is there an isolated track? (pT>10 GeV, reliso<0.1)
            passisotrkv2_ = passIsoTrkVeto_v2() ? 1 : 0; // is there an isolated track? (pT>10 GeV, reliso<0.1)
            nlep_       = stopt.ngoodlep();              // number of analysis selected leptons

            lep1pt_     = stopt.lep1().pt();             // 1st lepton pt
            lep1eta_    = fabs( stopt.lep1().eta() );    // 1st lepton eta

            if( nlep_ > 1 ){
                lep2pt_    = stopt.lep1().pt();            // 2nd lepton pt
                lep2eta_   = stopt.lep1().eta();           // 2nd lepton eta
                dilmass_   = stopt.dilmass();              // dilepton mass
            }

            if(indexBJets.size()>0) {
                dRleptB1_ = deltaR(stopt.pfjets().at(indexBJets.at(0)).eta() , stopt.pfjets().at(indexBJets.at(0)).phi() , stopt.lep1().eta(), stopt.lep1().phi());
            }

            // event shapes (from jets only)
            thrjet_    = thrust.thrust();
            apljet_    = eventshape.aplanarity();
            sphjet_    = eventshape.sphericity();
            cirjet_    = eventshape.circularity();

            // event shapes (from jets and lepton)
            thrjetl_   = thrustl.thrust();
            apljetl_   = eventshapel.aplanarity();
            sphjetl_   = eventshapel.sphericity();
            cirjetl_   = eventshapel.circularity();

            // event shapes (from jets, lepton, and MET)
            thrjetlm_   = thrustlm.thrust();
            apljetlm_   = eventshapelm.aplanarity();
            sphjetlm_   = eventshapelm.sphericity();
            cirjetlm_   = eventshapelm.circularity();

            // event shapes: HT in hemisphers
            htssl_      = HT_SSL;
            htosl_      = HT_OSL;
            htratiol_   = HT_SSL / (HT_OSL + HT_SSL);

            htssm_      = HT_SSM;
            htosm_      = HT_OSM;
            htratiom_   = HT_SSM / (HT_OSM + HT_SSM);

            dphimj1_    = getdphi(stopt.t1metphicorrphi(), jetVector.at(0).phi() );
            dphimj2_    = getdphi(stopt.t1metphicorrphi(), jetVector.at(1).phi() );
            dphimjmin_  = TMath::Min( dphimj1_ , dphimj2_ );

            rand_       = aleatorio.Uniform(0.0,1.0);

            for ( int i=0; i < 5; i++){
                bdt_[i] = 0;
                if ( __apply_mva && i>0 )
                    bdt_[i] = reader[i]->EvaluateMVA( "BDT" );
            }

            // fill me up
            nEventsPass++;
            outTree_->Fill();

            } // end event loop

            delete tree;
        } // end file loop

        //-------------------------
        // finish and clean up
        //-------------------------

        cout << "[StopTreeLooper::loop] saving mini-baby with total entries " << nEventsPass << endl;

        outFile_->cd();
        outTree_->Write();
        outFile_->Close();
        delete outFile_;

        already_seen.clear();

        gROOT->cd();

    }

    //--------------------------------------------
    // create the tree and set branch addresses
    //--------------------------------------------

    void StopTreeLooper::makeTree(const char *prefix, TChain* chain){
        TDirectory *rootdir = gDirectory->GetDirectory("Rint:");
        rootdir->cd();

        string revision = "$Revision: 1.32 $";
        string revision_no = revision.substr(11, revision.length() - 13);
        outFile_   = new TFile(Form("output/%s_mini_%s.root",prefix,revision_no.c_str()), "RECREATE");
        outFile_->cd();

        //  outTree_ = new TTree("t","Tree");

        outTree_ = chain->CloneTree(0);
//        if ( not __apply_mva ) {
            outTree_->Branch("mini_met"              ,        &met_             ,         "mini_met/F"		);

            outTree_->Branch("mini_sig"              ,        &sig_             ,         "mini_sig/I"		);
            outTree_->Branch("mini_cr1"              ,        &cr1_             ,         "mini_cr1/I"		);
            outTree_->Branch("mini_cr4"              ,        &cr4_             ,         "mini_cr4/I"		);
            outTree_->Branch("mini_cr5"              ,        &cr5_             ,         "mini_cr5/I"		);

            outTree_->Branch("mini_chi2"             ,        &chi2_            ,         "mini_chi2/F"          );
            outTree_->Branch("mini_mt2w"             ,        &mt2w_            ,         "mini_mt2w/F"          );

            outTree_->Branch("mini_weight"           ,        &weight_          ,         "mini_weight/F"		);
            outTree_->Branch("mini_sltrigeff"        ,        &sltrigeff_       ,         "mini_sltrigeff/F"	);
            outTree_->Branch("mini_dltrigeff"        ,        &dltrigeff_       ,         "mini_dltrigeff/F"	);

            outTree_->Branch("mini_nb"               ,        &nb_              ,         "mini_nb/I"		);
            outTree_->Branch("mini_njets"            ,        &njets_           ,         "mini_njets/I"		);

            outTree_->Branch("mini_passisotrk"       ,        &passisotrk_      ,         "mini_passisotrk/I"	);
            outTree_->Branch("mini_passisotrkv2"     ,        &passisotrkv2_    ,         "mini_passisotrkv2/I"	);

            outTree_->Branch("mini_nlep"             ,        &nlep_            ,         "mini_nlep/I"		);
            outTree_->Branch("mini_dRleptB1"         ,        &dRleptB1_        ,         "mini_dRleptB1/F"	);
            outTree_->Branch("mini_lep1pt"           ,        &lep1pt_          ,         "mini_lep1pt/F"		);
            outTree_->Branch("mini_lep1eta"          ,        &lep1eta_         ,         "mini_lep1eta/F"		);
            outTree_->Branch("mini_lep2pt"           ,        &lep2pt_          ,         "mini_lep2pt/F"		);
            outTree_->Branch("mini_lep2eta"          ,        &lep2eta_         ,         "mini_lep2eta/F"		);
            outTree_->Branch("mini_dilmass"          ,        &dilmass_         ,         "mini_dilmass/F"		);

            outTree_->Branch("mini_thrjet"           ,        &thrjet_          ,         "mini_thrjet/F"           );
            outTree_->Branch("mini_sphjet"           ,        &sphjet_          ,         "mini_sphjet/F"           );
            outTree_->Branch("mini_apljet"           ,        &apljet_          ,         "mini_apljet/F"           );
            outTree_->Branch("mini_cirjet"           ,        &cirjet_          ,         "mini_cirjet/F"           );
            outTree_->Branch("mini_thrjetl"          ,        &thrjetl_         ,         "mini_thrjetl/F"          );
            outTree_->Branch("mini_sphjetl"          ,        &sphjetl_         ,         "mini_sphjetl/F"          );
            outTree_->Branch("mini_apljetl"          ,        &apljetl_         ,         "mini_apljetl/F"          );
            outTree_->Branch("mini_cirjetl"          ,        &cirjetl_         ,         "mini_cirjetl/F"          );
            outTree_->Branch("mini_thrjetlm"         ,        &thrjetlm_        ,         "mini_thrjetlm/F"         );
            outTree_->Branch("mini_sphjetlm"         ,        &sphjetlm_        ,         "mini_sphjetlm/F"         );
            outTree_->Branch("mini_apljetlm"         ,        &apljetlm_        ,         "mini_apljetlm/F"         );
            outTree_->Branch("mini_cirjetlm"         ,        &cirjetlm_        ,         "mini_cirjetlm/F"         );
            outTree_->Branch("mini_htssl"            ,        &htssl_           ,         "mini_htssl/F"            );
            outTree_->Branch("mini_htosl"            ,        &htosl_           ,         "mini_htosl/F"            );
            outTree_->Branch("mini_htratiol"         ,        &htratiol_        ,         "mini_htraiol/F"          );
            outTree_->Branch("mini_htssm"            ,        &htssm_           ,         "mini_htssm/F"            );
            outTree_->Branch("mini_htosm"            ,        &htosm_           ,         "mini_htosm/F"            );
            outTree_->Branch("mini_htratiom"         ,        &htratiom_        ,         "mini_htraiom/F"          );
            outTree_->Branch("mini_dphimj1"          ,        &dphimj1_         ,         "mini_dphimj1/F"          );
            outTree_->Branch("mini_dphimj2"          ,        &dphimj2_         ,         "mini_dphimj2/F"          );
            outTree_->Branch("mini_dphimjmin"        ,        &dphimjmin_       ,         "mini_dphimjmin/F"        );

            outTree_->Branch("mini_pt_b"             ,        &pt_b_            ,         "mini_pt_b/F"             );
            outTree_->Branch("mini_pt_J1"            ,        &pt_J1_           ,         "mini_pt_J1/F"            );
            outTree_->Branch("mini_pt_J2"            ,        &pt_J2_           ,         "mini_pt_J2/F"            );

            outTree_->Branch("mini_rand"             ,        &rand_            ,         "mini_rand/F"             );

            outTree_->Branch("mini_t2ttLM"             ,        &t2ttLM_            ,         "mini_t2ttLM/F"          );
            outTree_->Branch("mini_t2ttHM"             ,        &t2ttHM_            ,         "mini_t2ttHM/F"          );
//        } else {
            outTree_->Branch("mini_bdt"              ,        &bdt_             ,         "mini_bdt[5]/F"           );
//        }
    }

    //--------------------------------------------
    // set all branches to -1
    //--------------------------------------------

    void StopTreeLooper::initBaby(){

        // which selections are passed
        sig_        = -1;
        cr1_        = -1;
        cr4_        = -1;
        cr5_        = -1; 

        // kinematic variables
        met_        = -1.0;
        chi2_       = -1.0;
        mt2w_       = -1.0;

        // event shapes
        thrjet_     = -1.0;
        apljet_     = -1.0;
        sphjet_     = -1.0;
        cirjet_     = -1.0;

        thrjetl_    = -1.0;
        apljetl_    = -1.0;
        sphjetl_    = -1.0;
        cirjetl_    = -1.0;

        thrjetlm_   = -1.0;
        apljetlm_   = -1.0;
        sphjetlm_   = -1.0;
        cirjetlm_   = -1.0;

        htssl_      = -1.0;
        htosl_      = -1.0;
        htratiol_   = -1.0;
        htssm_      = -1.0;
        htosm_      = -1.0;
        htratiom_   = -1.0;

        // weights
        weight_     = -1.0;
        sltrigeff_  = -1.0;
        dltrigeff_  = -1.0;

        // hadronic variables
        nb_         = -1;
        njets_      = -1;

        // lepton variables
        passisotrk_ = -1;
        passisotrkv2_ = -1;

        nlep_       = -1;
        dRleptB1_   = -1;
        lep1pt_     = -1.0;
        lep1eta_    = -1.0;
        lep2pt_     = -1.0;
        lep2eta_    = -1.0;
        dilmass_    = -1.0;

        // jet kinematics
        pt_b_ = -1.0;
        pt_J1_ = -1.0;
        pt_J2_ = -1.0;

        rand_       = -1.0;

        t2ttLM_ =-1;
        t2ttLM_ =-1;

        bdt_[0] = -1;
        bdt_[1] = -1;
        bdt_[2] = -1;
        bdt_[3] = -1;
        bdt_[4] = -1;
    }

