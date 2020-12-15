#include "AnalysisEvent.hpp"
#include "TChain.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TProfile.h"
#include "TLegend.h"
#include "TStyle.h"
#include "TASImage.h"
#include "TLatex.h"
#include "TMVA/Timer.h"
#include "TTree.h"
#include "TLorentzVector.h"
#include "TString.h"
#include "config_parser.hpp"

#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/program_options.hpp>
#include <boost/range/iterator_range.hpp>

#include <algorithm> 
#include <chrono> 
#include <fstream>
#include <iostream>
#include <regex>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <map>


std::vector<int> getLooseMuons(const AnalysisEvent& event);
std::vector<int> getPromptMuons(const AnalysisEvent& event, const std::vector<int>& muonIndex, const bool getPrompt );
bool getDileptonCand(AnalysisEvent& event, const std::vector<int>& muons, const float scalarMass, const bool exactlyTwo);
bool scalarGrandparent(const AnalysisEvent& event, const Int_t& k, const Int_t& pdgId_);
float deltaR(float eta1, float phi1, float eta2, float phi2);

namespace fs = boost::filesystem;

// Lepton cut variables
const float looseMuonEta_ {2.8}, looseMuonPt_ {6.}, looseMuonPtLeading_ {15.}, looseMuonRelIso_ {100.};
const float invZMassCut_ {10.0};

int main(int argc, char* argv[])
{
    auto timerStart = std::chrono::high_resolution_clock::now(); 

    std::string config;
    std::vector<Dataset> datasets;
    double totalLumi;
    double usePreLumi;
    bool usePostLepTree {false};
    float scalarMass_;
    bool exactlyTwo_ {false};
   
    std::string outFileString{"plots/distributions/output.root"};
    bool is2016_;
    Long64_t nEvents;
    Long64_t totalEvents {0};
    const std::regex mask{".*\\.root"};

    // Quick and dirty trigger plots
    // denom
    TH1F* h_leadingMuonPt_truth            {new TH1F("h_leadingMuonPt_truth",      "", 200, 0., 100.)};
    TH1F* h_subLeadingMuonPt_truth         {new TH1F("h_subLeadingMuonPt_truth",   "", 200, 0., 100.)};
    TH1F* h_leadingMuonPt                  {new TH1F("h_leadingMuonPt",            "", 200, 0., 100.)};
    TH1F* h_subLeadingMuonPt               {new TH1F("h_subLeadingMuonPt",         "", 200, 0., 100.)};
    TH1F* h_delR_truth                     {new TH1F("h_delR_truth",               "", 100, 0., 1.0)};
    TH1F* h_delR                           {new TH1F("h_delR",                     "", 100, 0., 1.0)};
    // numerator - single mu
    TH1F* h_leadingMuonPt_truth_muTrig     {new TH1F("h_leadingMuonPt_truth_muTrig",      "Trigger turn-on for signal; p_{T} (GeV); #mu trigger #epsilon", 200, 0., 100.)};
    TH1F* h_subLeadingMuonPt_truth_muTrig  {new TH1F("h_subLeadingMuonPt_truth_muTrig",   "Trigger turn-on for signal; p_{T} (GeV); #mu trigger #epsilon", 200, 0., 100.)};
    TH1F* h_leadingMuonPt_muTrig           {new TH1F("h_leadingMuonPt_muTrig",            "Trigger turn-on for signal; p_{T} (GeV); #mu trigger #epsilon", 200, 0., 100.)};
    TH1F* h_subLeadingMuonPt_muTrig        {new TH1F("h_subLeadingMuonPt_muTrig",         "Trigger turn-on for signal; p_{T} (GeV); #mu trigger #epsilon", 200, 0., 100.)};
    TH1F* h_delR_truth_muTrig              {new TH1F("h_delR_truth_muTrig",               "Trigger turn-on for signal; #Delta R; #mu trigger #epsilon", 100, 0., 1.0)};
    TH1F* h_delR_muTrig                    {new TH1F("h_delR_muTrig",                     "Trigger turn-on for signal; #Delta R; #mu trigger #epsilon", 100, 0., 1.0)};
    // numerator - doubele mu
    TH1F* h_leadingMuonPt_truth_mumuTrig       {new TH1F("h_leadingMuonPt_truth_mumuTrig",      "Trigger turn-on for signal; p_{T} (GeV); #mu#mu trigger #epsilon", 200, 0., 100.)};
    TH1F* h_subLeadingMuonPt_truth_mumuTrig    {new TH1F("h_subLeadingMuonPt_truth_mumuTrig",   "Trigger turn-on for signal; p_{T} (GeV); #mu#mu trigger #epsilon", 200, 0., 100.)};
    TH1F* h_leadingMuonPt_mumuTrig             {new TH1F("h_leadingMuonPt_mumuTrig",            "Trigger turn-on for signal; p_{T} (GeV); #mu#mu trigger #epsilon", 200, 0., 100.)};
    TH1F* h_subLeadingMuonPt_mumuTrig          {new TH1F("h_subLeadingMuonPt_mumuTrig",         "Trigger turn-on for signal; p_{T} (GeV); #mu#mu trigger #epsilon", 200, 0., 100.)};
    TH1F* h_delR_truth_mumuTrig                {new TH1F("h_delR_truth_mumuTrig",               "Trigger turn-on for signal; #Delta R; #mu#mu trigger #epsilon", 100, 0., 1.0)};
    TH1F* h_delR_mumuTrig                      {new TH1F("h_delR_mumuTrig",                    "Trigger turn-on for signal;  #Delta R; #mu#mu trigger #epsilon", 100, 0., 1.0)};
    // numerator - L2 mu
    TH1F* h_leadingMuonPt_truth_L2muTrig       {new TH1F("h_leadingMuonPt_truth_L2muTrig",      "Trigger turn-on for signal; p_{T} (GeV); L2 #mu#mu trigger #epsilon", 200, 0., 100.)};
    TH1F* h_subLeadingMuonPt_truth_L2muTrig    {new TH1F("h_subLeadingMuonPt_truth_L2muTrig",   "Trigger turn-on for signal; p_{T} (GeV); L2 #mu#mu trigger #epsilon", 200, 0., 100.)};
    TH1F* h_leadingMuonPt_L2muTrig             {new TH1F("h_leadingMuonPt_L2muTrig",            "Trigger turn-on for signal; p_{T} (GeV); L2 #mu#mu trigger #epsilon", 200, 0., 100.)};
    TH1F* h_subLeadingMuonPt_L2muTrig          {new TH1F("h_subLeadingMuonPt_L2muTrig",         "Trigger turn-on for signal; p_{T} (GeV); L2 #mu#mu trigger #epsilon", 200, 0., 100.)};
    TH1F* h_delR_truth_L2muTrig                {new TH1F("h_delR_truth_L2muTrig",               "Trigger turn-on for signal; #Delta R; L2 #mu#	mu trigger #epsilon", 100, 0., 1.0)};
    TH1F* h_delR_L2muTrig                      {new TH1F("h_delR_L2muTrig",                    "Trigger turn-on for signal; #Delta R;  L2 #mu#mu trigger #epsilon", 100, 0., 1.0)};

    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()("help,h", "Print this message.")(
        "config,c",
        po::value<std::string>(&config)->required(),
        "The configuration file to be used.")(
        "lumi,l",
        po::value<double>(&usePreLumi)->default_value(41528.0),
        "Lumi to scale MC plots to.")(
        "outfile,o",
        po::value<std::string>(&outFileString)->default_value(outFileString),
        "Output file for plots.")(
        ",n",
        po::value<Long64_t>(&nEvents)->default_value(0),
        "The number of events to be run over. All if set to 0.")(
        ",u",
        po::bool_switch(&usePostLepTree),
        "Use post lepton selection trees.")(
        "2016", po::bool_switch(&is2016_), "Use 2016 conditions (SFs, et al.).")(
        "scalarMass,s",
        po::value<float>(&scalarMass_)->default_value(2.0),
        "scalar mass being searched for.")(
        ",e", po::bool_switch(&exactlyTwo_), "only use events with exactly two loose muons");
    po::variables_map vm;

    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help"))
        {
            std::cout << desc;
            return 0;
        }

        po::notify(vm);
    }
    catch (po::error& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    // Some vectors that will be filled in the parsing
    totalLumi = 0;

    try
    {
        Parser::parse_config(config,
                             datasets,
                             totalLumi,
                             usePostLepTree);
    }
    catch (const std::exception)
    {
        std::cerr << "ERROR Problem with a confugration file, see previous "
                     "errors for more details. If this is the only error, the "
                     "problem is with the main configuration file."
                  << std::endl;
        throw;
    }

    if (totalLumi == 0.)
    {
        totalLumi = usePreLumi;
    }
    std::cout << "Using lumi: " << totalLumi << std::endl;

    bool datasetFilled{false};
    const std::string postLepSelSkimInputDir{std::string{"/pnfs/iihe/cms/store/user/almorton/MC/postLepSkims/postLepSkims"} + (is2016_ ? "2016" : "2017") + "/"};
//    const std::string postLepSelSkimInputDir{std::string{"/user/almorton/HToSS_analysis/postLepSkims"} + (is2016_ ? "2016" : "2017") + "/"};

    // Begin to loop over all datasets
    for (auto dataset = datasets.begin(); dataset != datasets.end(); ++dataset)
    {
        datasetFilled = false;
        TChain* datasetChain{new TChain{dataset->treeName().c_str()}};
        datasetChain->SetAutoSave(0);

        std::cerr << "Processing dataset " << dataset->name() << std::endl;
        if (!usePostLepTree) {
            if (!datasetFilled) {
                if (!dataset->fillChain(datasetChain)) {
                    std::cerr << "There was a problem constructing the chain for " << dataset->name() << ". Continuing with next dataset.\n";
                    continue;
                }
                datasetFilled=true;
            }
        }
        else {
            std::cout << postLepSelSkimInputDir + dataset->name() + "mumuSmallSkim.root" << std::endl;
            datasetChain->Add((postLepSelSkimInputDir + dataset->name() + "mumuSmallSkim.root").c_str());
        }

        // extract the dataset weight. MC = (lumi*crossSection)/(totalEvents), data = 1.0
        float datasetWeight{dataset->getDatasetWeight(totalLumi)};
        std::cout << datasetChain->GetEntries() << " number of items in tree. Dataset weight: " << datasetWeight << std::endl;
        if (datasetChain->GetEntries() == 0) {
            std::cout << "No entries in tree, skipping..." << std::endl;
            continue;
        }

        AnalysisEvent event{dataset->isMC(), datasetChain, is2016_};

        Long64_t numberOfEvents{datasetChain->GetEntries()};
        if (nEvents && nEvents < numberOfEvents) numberOfEvents = nEvents;

        TMVA::Timer* lEventTimer{ new TMVA::Timer{boost::numeric_cast<int>(numberOfEvents), "Running over dataset ...", false}}; 
        lEventTimer->DrawProgressBar(0, "");
    
        totalEvents += numberOfEvents;
        for (Long64_t i{0}; i < numberOfEvents; i++) {

            lEventTimer->DrawProgressBar(i,"");

            event.GetEntry(i);

            float eventWeight = 1.;

            if (!event.metFilters()) continue;

            const bool passSingleMuonTrigger {event.muTrig()}, passDimuonTrigger {event.mumuTrig()};
            const bool passL2MuonTrigger {event.mumuL2Trig()}, passDimuonNoVtxTrigger {event.mumuNoVtxTrig()};

            if ( event.numMuonPF2PAT > 1 ) {
                // fill muon pT plots pre-triggers
                //// ID requirements PF muon? no pT cut
                //// reco pT 
                int mu1 {-1}, mu2{-1};
                for ( Int_t k{0}; k < event.numMuonPF2PAT; k++ ) {
                    if ( event.genMuonPF2PATMotherId[k] == 9000006 && mu1 < 0 ) mu1 = k;
                    else if ( event.genMuonPF2PATMotherId[k] == 9000006 && mu2 < 0 ) mu2 = k;
                    else if (mu1 >= 0 && mu2 > 0) break;
                }

                TLorentzVector muon1, muon2; 
	
                float delR_truth = deltaR(event.muonPF2PATEta[mu1], event.muonPF2PATPhi[mu1], event.muonPF2PATEta[mu2], event.muonPF2PATPhi[mu2]);
                float delR = deltaR(event.muonPF2PATEta[0], event.muonPF2PATPhi[0], event.muonPF2PATEta[1], event.muonPF2PATPhi[1]);

                // Fill general pT/dR (with and without scalar parentage)
                h_leadingMuonPt_truth->Fill(event.muonPF2PATPt[mu1]);
                h_subLeadingMuonPt_truth->Fill(event.muonPF2PATPt[mu2]);
                h_leadingMuonPt->Fill(event.muonPF2PATPt[0]);
                h_subLeadingMuonPt->Fill(event.muonPF2PATPt[1]);
                h_delR_truth->Fill(delR_truth);
                h_delR->Fill(delR);
                // Fill pT post trigger (with and without scalar parentage)
                if (passSingleMuonTrigger) {
                    h_leadingMuonPt_truth_muTrig->Fill(event.muonPF2PATPt[mu1]);
                    h_subLeadingMuonPt_truth_muTrig->Fill(event.muonPF2PATPt[mu2]);
                    h_leadingMuonPt_muTrig->Fill(event.muonPF2PATPt[0]);
                    h_subLeadingMuonPt_muTrig->Fill(event.muonPF2PATPt[1]);
                    h_delR_truth_muTrig->Fill(delR_truth);
                    h_delR_muTrig->Fill(delR);
                }
                if (passDimuonTrigger) {
                    h_leadingMuonPt_truth_mumuTrig->Fill(event.muonPF2PATPt[mu1]);
                    h_subLeadingMuonPt_truth_mumuTrig->Fill(event.muonPF2PATPt[mu2]);
                    h_leadingMuonPt_mumuTrig->Fill(event.muonPF2PATPt[0]);
                    h_subLeadingMuonPt_mumuTrig->Fill(event.muonPF2PATPt[1]);
                    h_delR_truth_mumuTrig->Fill(delR_truth);
                    h_delR_mumuTrig->Fill(delR);
                }
                if (passL2MuonTrigger || passDimuonNoVtxTrigger) {
                    h_leadingMuonPt_truth_L2muTrig->Fill(event.muonPF2PATPt[mu1]);
                    h_subLeadingMuonPt_truth_L2muTrig->Fill(event.muonPF2PATPt[mu2]);
                    h_leadingMuonPt_L2muTrig->Fill(event.muonPF2PATPt[0]);
                    h_subLeadingMuonPt_L2muTrig->Fill(event.muonPF2PATPt[1]);
                    h_delR_truth_L2muTrig->Fill(delR_truth);
                    h_delR_L2muTrig->Fill(delR);
                }
            }

            if (! ( passDimuonTrigger || passSingleMuonTrigger ) ) continue;

            std::vector<int> looseMuonIndex = getLooseMuons(event);

//            std::vector<int> promptLooseMuonIndex     = getPromptMuons(event, looseMuonIndex, true);
//            std::vector<int> nonpromptLooseMuonIndex  = getPromptMuons(event, looseMuonIndex, false);


        } // end event loop
    } // end dataset loop

    TFile* outFile{new TFile{outFileString.c_str(), "RECREATE"}};
    outFile->cd();

    h_leadingMuonPt_truth_muTrig->Divide(h_leadingMuonPt_truth);
    h_subLeadingMuonPt_truth_muTrig->Divide(h_subLeadingMuonPt_truth);
    h_leadingMuonPt_muTrig->Divide(h_leadingMuonPt);
    h_subLeadingMuonPt_muTrig->Divide(h_subLeadingMuonPt);
    h_delR_truth_muTrig->Divide(h_delR_truth);
    h_delR_muTrig->Divide(h_delR);

    h_leadingMuonPt_truth_mumuTrig->Divide(h_leadingMuonPt_truth);
    h_subLeadingMuonPt_truth_mumuTrig->Divide(h_subLeadingMuonPt_truth);
    h_leadingMuonPt_mumuTrig->Divide(h_leadingMuonPt);
    h_subLeadingMuonPt_mumuTrig->Divide(h_subLeadingMuonPt);
    h_delR_truth_mumuTrig->Divide(h_delR_truth);
    h_delR_mumuTrig->Divide(h_delR);

    h_leadingMuonPt_truth_L2muTrig->Divide(h_leadingMuonPt_truth);
    h_subLeadingMuonPt_truth_L2muTrig->Divide(h_subLeadingMuonPt_truth);
    h_leadingMuonPt_L2muTrig->Divide(h_leadingMuonPt);
    h_subLeadingMuonPt_L2muTrig->Divide(h_subLeadingMuonPt);
    h_delR_truth_L2muTrig->Divide(h_delR_truth);
    h_delR_L2muTrig->Divide(h_delR);

    h_leadingMuonPt_truth_muTrig->Write();
    h_subLeadingMuonPt_truth_muTrig->Write();
    h_leadingMuonPt_muTrig->Write();
    h_subLeadingMuonPt_muTrig->Write();
    h_delR_truth_muTrig->Write();
    h_delR_muTrig->Write();
    h_leadingMuonPt_truth_mumuTrig->Write();
    h_subLeadingMuonPt_truth_mumuTrig->Write();
    h_leadingMuonPt_mumuTrig->Write();
    h_subLeadingMuonPt_mumuTrig->Write();
    h_delR_truth_mumuTrig->Write();
    h_delR_mumuTrig->Write();
    h_leadingMuonPt_truth_L2muTrig->Write();
    h_subLeadingMuonPt_truth_L2muTrig->Write();
    h_leadingMuonPt_L2muTrig->Write();
    h_subLeadingMuonPt_L2muTrig->Write();
    h_delR_truth_L2muTrig->Write();
    h_delR_L2muTrig->Write();

    outFile->Close();

//    std::cout << "Max nGenPar: " << maxGenPars << std::endl;    
    auto timerStop = std::chrono::high_resolution_clock::now(); 
    auto duration  = std::chrono::duration_cast<std::chrono::seconds>(timerStop - timerStart);

    std::cout << "\nFinished. Took " << duration.count() << " seconds" <<std::endl;
}

std::vector<int> getLooseMuons(const AnalysisEvent& event) {
    std::vector<int> muons;
    for (int i{0}; i < event.numMuonPF2PAT; i++)  {
       if (event.muonPF2PATIsPFMuon[i] && event.muonPF2PATLooseCutId[i] /*&& event.muonPF2PATPfIsoLoose[i]*/ && std::abs(event.muonPF2PATEta[i]) < looseMuonEta_) {
           if (event.muonPF2PATPt[i] >= (muons.empty() ? looseMuonPtLeading_ : looseMuonPt_)) muons.emplace_back(i);
        }
    }
    return muons;
}

std::vector<int> getPromptMuons(const AnalysisEvent& event, const std::vector<int>& muonIndex, const bool getPrompt ) {
    std::vector<int> muons;
    for ( auto it = muonIndex.begin(); it!= muonIndex.end(); it++ ) {
        if ( event.genMuonPF2PATHardProcess[*it] == getPrompt ) muons.push_back(*it);
    }
    return muons;
}

bool getDileptonCand(AnalysisEvent& event, const std::vector<int>& muons, const float skMass, const bool exactlyTwo) {
    if (exactlyTwo && muons.size() == 2) {

        if (event.muonPF2PATCharge[muons[0]] * event.muonPF2PATCharge[muons[1]] >= 0) return false;

        TLorentzVector lepton1{event.muonPF2PATPX[muons[0]],
                               event.muonPF2PATPY[muons[0]],
                               event.muonPF2PATPZ[muons[0]],
                               event.muonPF2PATE[muons[0]]};
        TLorentzVector lepton2{event.muonPF2PATPX[muons[1]],
                               event.muonPF2PATPY[muons[1]],
                               event.muonPF2PATPZ[muons[1]],
                               event.muonPF2PATE[muons[1]]};

        event.zPairLeptons.first = lepton1.Pt() > lepton2.Pt() ? lepton1 : lepton2;
        event.zPairIndex.first = lepton1.Pt() > lepton2.Pt() ? muons[0] : muons[1];
        event.zPairRelIso.first = event.muonPF2PATComRelIsodBeta[muons[0]];
        event.zPairLeptons.second = lepton1.Pt() > lepton2.Pt() ? lepton2 : lepton1;
        event.zPairRelIso.second = event.muonPF2PATComRelIsodBeta[muons[1]];
        event.zPairIndex.second = lepton1.Pt() > lepton2.Pt() ? muons[1] : muons[0];
        return true;
    }
    else if (!exactlyTwo && muons.size() > 1) {
        double closestMass {9999.};
        for ( unsigned int i{0}; i < muons.size(); i++ ) {
            for ( unsigned int j{i+1}; j < muons.size(); j++ ) {
                if (event.muonPF2PATCharge[i] * event.muonPF2PATCharge[j] >= 0) continue;
                TLorentzVector lepton1{event.muonPF2PATPX[i],
                                       event.muonPF2PATPY[i],
                                       event.muonPF2PATPZ[i],
                                       event.muonPF2PATE[i]};
                TLorentzVector lepton2{event.muonPF2PATPX[j],
                                       event.muonPF2PATPY[j],
                                       event.muonPF2PATPZ[j],
                                       event.muonPF2PATE[j]};
                double invMass { (lepton1+lepton2).M() };
                if ( std::abs(( invMass - skMass )) < std::abs(closestMass) ) {
                    event.zPairLeptons.first = lepton1.Pt() > lepton2.Pt() ? lepton1 : lepton2;
                    event.zPairIndex.first = lepton1.Pt() > lepton2.Pt() ? muons[0] : muons[1];
                    event.zPairRelIso.first = event.muonPF2PATComRelIsodBeta[muons[0]];
                    event.zPairLeptons.second = lepton1.Pt() > lepton2.Pt() ? lepton2 : lepton1;
                    event.zPairRelIso.second = event.muonPF2PATComRelIsodBeta[muons[1]];
                    event.zPairIndex.second = lepton1.Pt() > lepton2.Pt() ? muons[1] : muons[0];
                    closestMass = ( invMass - skMass );
                }
            }
        }
        if ( closestMass < 9999. ) return true;
    }
    return false;
}


bool scalarGrandparent (const AnalysisEvent& event, const Int_t& k, const Int_t& grandparentId) {

    const Int_t pdgId        { std::abs(event.genParId[k]) };
    const Int_t numDaughters { event.genParNumDaughters[k] };
    const Int_t motherId     { std::abs(event.genParMotherId[k]) };
    const Int_t motherIndex  { std::abs(event.genParMotherIndex[k]) };


    if (motherId == 0 || motherIndex == -1) return false; // if no parent, then mother Id is null and there's no index, quit search
    else if (motherId == std::abs(grandparentId)) return true; // if mother is granparent being searched for, return true
    else if (motherIndex > event.NGENPARMAX) return false; // index exceeds stored genParticle range, return false for safety
    else {
//        std::cout << "Going up the ladder ... pdgId = " << pdgId << " : motherIndex = " << motherIndex << " : motherId = " << motherId << std::endl;
//        debugCounter++;
//        std::cout << "debugCounter: " << debugCounter << std::endl;
        return scalarGrandparent(event, motherIndex, grandparentId); // otherwise check mother's mother ...
    }
}

float deltaR(float eta1, float phi1, float eta2, float phi2){
  float dEta = eta1-eta2;
  float dPhi = phi1-phi2;
  while (fabs(dPhi) > 3.14159265359){
    dPhi += (dPhi > 0.? -2*3.14159265359:2*3.14159265359);
  }
  //  if(singleEventInfoDump_)  std::cout << eta1 << " " << eta2 << " phi " << phi1 << " " << phi2 << " ds: " << eta1-eta2 << " " << phi1-phi2 << " dR: " << std::sqrt((dEta*dEta)+(dPhi*dPhi)) << std::endl;
  return std::sqrt((dEta*dEta)+(dPhi*dPhi));
}
