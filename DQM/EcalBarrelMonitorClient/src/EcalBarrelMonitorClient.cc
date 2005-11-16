/*
 * \file EcalBarrelMonitorClient.cc
 * 
 * $Date: 2005/11/15 21:02:45 $
 * $Revision: 1.21 $
 * \author G. Della Ricca
 *
*/

#include <DQM/EcalBarrelMonitorClient/interface/EcalBarrelMonitorClient.h>

EcalBarrelMonitorClient::EcalBarrelMonitorClient(const edm::ParameterSet& ps){

  cout << endl;
  cout << " *** Ecal Barrel Generic Monitor Client ***" << endl;
  cout << endl;

  mui_ = 0;
  econn_ = 0;

  h_ = 0;

  // default client name
  clientName_ = ps.getUntrackedParameter<string>("clientName", "EcalBarrelMonitorClient");

  // default collector host name
  hostName_ = ps.getUntrackedParameter<string>("hostName", "localhost");

  // default host port
  hostPort_ = ps.getUntrackedParameter<int>("hostPort", 9090);;

  cout << " Client " << clientName_
       << " begins requesting monitoring from host " << hostName_
       << " on port " << hostPort_ << endl;

  // start user interface instance
  mui_ = new MonitorUIRoot(hostName_, hostPort_, clientName_);

  mui_->setVerbose(1);

  // will attempt to reconnect upon connection problems (w/ a 5-sec delay)
  mui_->setReconnectDelay(5);

  integrity_client_ = new EBIntegrityClient(ps, mui_);
  laser_client_ = new EBLaserClient(ps, mui_);
  pedestal_client_ = new EBPedestalClient(ps, mui_);
  pedpresample_client_ = new EBPedPreSampleClient(ps, mui_);
  testpulse_client_ = new EBTestPulseClient(ps, mui_);

  // Ecal Cond DB
  dbName_ = ps.getUntrackedParameter<string>("dbName", "");
  dbHostName_ = ps.getUntrackedParameter<string>("dbHostName", "");
  dbUserName_ = ps.getUntrackedParameter<string>("dbUserName", "");
  dbPassword_ = ps.getUntrackedParameter<string>("dbPassword", "");

  if ( dbName_.size() != 0 ) {
    cout << " DB output will go to"
         << " dbName = " << dbName_
         << " dbHostName = " << dbHostName_
         << " dbUserName = " << dbUserName_ << endl;
  } else {
    cout << " DB output is disabled" << endl;
  }

  // base Html output directory
  baseHtmlDir_ = ps.getUntrackedParameter<string>("baseHtmlDir", ".");

  if ( baseHtmlDir_.size() != 0 ) {
    cout << " HTML output will go to"
         << " baseHtmlDir = " << baseHtmlDir_ << endl;
  } else {
    cout << " HTML output is disabled" << endl;
  }

}

EcalBarrelMonitorClient::~EcalBarrelMonitorClient(){

  cout << "Exit ..." << endl;

  this->unsubscribe();

  if ( h_ ) delete h_;

  if ( integrity_client_ ) delete integrity_client_;
  if ( laser_client_ ) delete laser_client_;
  if ( pedestal_client_ ) delete pedestal_client_;
  if ( pedpresample_client_ ) delete pedpresample_client_;
  if ( testpulse_client_ ) delete testpulse_client_;

  usleep(100);

  if ( mui_ ) delete mui_;

}

void EcalBarrelMonitorClient::beginJob(const edm::EventSetup& c){

  cout << "EcalBarrelMonitorClient: beginJob" << endl;

  ievt_ = 0;

  this->beginRun(c);

  last_update_ = -1;

  integrity_client_->beginJob(c);
  laser_client_->beginJob(c);
  pedestal_client_->beginJob(c);
  pedpresample_client_->beginJob(c);
  testpulse_client_->beginJob(c);

}

void EcalBarrelMonitorClient::beginRun(const edm::EventSetup& c){
  
  cout << "EcalBarrelMonitorClient: beginRun" << endl;

  jevt_ = 0;

  last_jevt_ = -1;

  this->subscribe();

  status_  = "unknown";
  run_     = 0;
  evt_     = 0;
  runtype_ = "unknown";

  integrity_client_->beginRun(c);
  laser_client_->beginRun(c);
  pedestal_client_->beginRun(c);
  pedpresample_client_->beginRun(c);
  testpulse_client_->beginRun(c);

}

void EcalBarrelMonitorClient::endJob(void) {

  cout << "EcalBarrelMonitorClient: endJob, ievt = " << ievt_ << endl;

  integrity_client_->endJob();
  laser_client_->endJob();
  pedestal_client_->endJob();
  pedpresample_client_->endJob();
  testpulse_client_->endJob();

}

void EcalBarrelMonitorClient::endRun(void) {

  cout << "EcalBarrelMonitorClient: endRun, jevt = " << jevt_ << endl;

  mui_->save("EcalBarrelMonitorClient.root");

  econn_ = 0;

  if ( dbName_.size() != 0 ) {
    try {
      cout << "Opening DB connection." << endl;
      econn_ = new EcalCondDBInterface(dbHostName_, dbName_, dbUserName_, dbPassword_);
    } catch (runtime_error &e) {
      cerr << e.what() << endl;
    }
  }

  // The objects necessary to identify a dataset
  runiov_ = new RunIOV();
  runtag_ = new RunTag();

  Tm startTm;

  // Set the beginning time
  startTm.setToCurrentGMTime();
  startTm.setToMicrosTime(startTm.microsTime());

  cout << "Setting run " << run_ << " start_time " << startTm.str() << endl;

  runiov_->setRunNumber(run_);
  runiov_->setRunStart(startTm);

  runtag_->setRunType(runtype_);
  runtag_->setLocation(location_);
  runtag_->setMonitoringVersion("version 1");

  integrity_client_->endRun(econn_, runiov_, runtag_);
  laser_client_->endRun(econn_, runiov_, runtag_);
  pedestal_client_->endRun(econn_, runiov_, runtag_);
  pedpresample_client_->endRun(econn_, runiov_, runtag_);
  testpulse_client_->endRun(econn_, runiov_, runtag_);

  if ( econn_ ) {
    try {
      cout << "Closing DB connection." << endl;
      delete econn_;
      econn_ = 0;
    } catch (runtime_error &e) {
      cerr << e.what() << endl;
    }
  }

  if ( runiov_ ) delete runiov_;
  if ( runtag_ ) delete runtag_;

  if ( baseHtmlDir_.size() != 0 ) this->htmlOutput();

}

void EcalBarrelMonitorClient::subscribe(void){

  // subscribe to monitorable matching pattern
  mui_->subscribe("*/EcalBarrel/STATUS");
  mui_->subscribe("*/EcalBarrel/RUN");
  mui_->subscribe("*/EcalBarrel/EVT");
  mui_->subscribe("*/EcalBarrel/EVTTYPE");
  mui_->subscribe("*/EcalBarrel/RUNTYPE");

}

void EcalBarrelMonitorClient::subscribeNew(void){

  // subscribe to new monitorable matching pattern
  mui_->subscribeNew("*/EcalBarrel/STATUS");
  mui_->subscribeNew("*/EcalBarrel/RUN");
  mui_->subscribeNew("*/EcalBarrel/EVT");
  mui_->subscribeNew("*/EcalBarrel/EVTTYPE");
  mui_->subscribeNew("*/EcalBarrel/RUNTYPE");

}

void EcalBarrelMonitorClient::unsubscribe(void) {

  // subscribe to all monitorable matching pattern 
  mui_->unsubscribe("*/EcalBarrel/STATUS");
  mui_->unsubscribe("*/EcalBarrel/RUN");
  mui_->unsubscribe("*/EcalBarrel/EVT");
  mui_->unsubscribe("*/EcalBarrel/EVTTYPE");
  mui_->unsubscribe("*/EcalBarrel/RUNTYPE");

}

void EcalBarrelMonitorClient::analyze(const edm::Event& e, const edm::EventSetup& c){

  ievt_++;
  jevt_++;
  if ( ievt_ % 10 == 0 )
  cout << "EcalBarrelMonitorClient: ievt/jevt = " << ievt_ << "/" << jevt_ << endl;

  string s;

  bool stay_in_loop = mui_->update();

  this->subscribeNew();

  integrity_client_->subscribeNew();
  laser_client_->subscribeNew();
  pedestal_client_->subscribeNew();
  pedpresample_client_->subscribeNew();
  testpulse_client_->subscribeNew();

  // # of full monitoring cycles processed
  int updates = mui_->getNumUpdates();

  if ( stay_in_loop && updates != last_update_ ) {

    MonitorElement* me;

    me = mui_->get("Collector/FU0/EcalBarrel/STATUS");
    if ( me ) {
      s = me->valueString();
      if ( s.substr(2,1) == "0" ) status_ = "begin-of-run";
      if ( s.substr(2,1) == "1" ) status_ = "running";
      if ( s.substr(2,1) == "2" ) status_ = "end-of-run";
    }

    me = mui_->get("Collector/FU0/EcalBarrel/RUN");
    if ( me ) {
      s = me->valueString();
      sscanf((s.substr(2,s.length()-2)).c_str(), "%d", &run_);
    }

    me = mui_->get("Collector/FU0/EcalBarrel/EVT");
    if ( me ) {
      s = me->valueString();
      sscanf((s.substr(2,s.length()-2)).c_str(), "%d", &evt_);
    }

    me = mui_->get("Collector/FU0/EcalBarrel/EVTTYPE");
    if ( me ) {
      MonitorElementT<TNamed>* ob = dynamic_cast<MonitorElementT<TNamed>*> (me);
      if ( ob ) {
        if ( h_ ) delete h_;
        h_ = dynamic_cast<TH1F*> ((ob->operator->())->Clone());
      }
    }

    me = mui_->get("Collector/FU0/EcalBarrel/RUNTYPE");
    if ( me ) {
      s = me->valueString();
      if ( s.substr(2,1) == "0" ) runtype_ = "cosmic";
      if ( s.substr(2,1) == "1" ) runtype_ = "laser";
      if ( s.substr(2,1) == "2" ) runtype_ = "pedestal";
      if ( s.substr(2,1) == "3" ) runtype_ = "testpulse";
    }

    location_ = "H4";

    cout << " run = "      << run_      <<
            " event = "    << evt_      << endl;
    cout << " updates = "  << updates   <<
            " status = "   << status_   <<
            " runtype = " << runtype_  <<
            " location = " << location_ << endl;

    if ( h_ ) {
      cout << " event type = " << flush;
      for ( int i = 1; i <=10; i++ ) {
        cout << h_->GetBinContent(i) << " " << flush;
      }
      cout << endl;
    }

    if ( status_ == "begin-of-run" ) {
      this->beginRun(c);
    }

    if ( status_ == "running" ) {
      if ( updates != 0 && updates % 50 == 0 ) {
                                               integrity_client_->analyze(e, c);
        if ( h_ && h_->GetBinContent(2) != 0 ) laser_client_->analyze(e, c);
        if ( h_ && h_->GetBinContent(3) != 0 ) pedestal_client_->analyze(e, c);
                                               pedpresample_client_->analyze(e, c);
        if ( h_ && h_->GetBinContent(4) != 0 ) testpulse_client_->analyze(e, c);
      }
    }

    if ( status_ == "end-of-run" ) {
                                             integrity_client_->analyze(e, c);
      if ( h_ && h_->GetBinContent(2) != 0 ) laser_client_->analyze(e, c);
      if ( h_ && h_->GetBinContent(3) != 0 ) pedestal_client_->analyze(e, c);
                                             pedpresample_client_->analyze(e, c);
      if ( h_ && h_->GetBinContent(4) != 0 ) testpulse_client_->analyze(e, c);
      this->endRun();
    }

    if ( updates != 0 && updates % 100 == 0 ) {
      mui_->save("EcalBarrelMonitorClient.root");
    }

    last_update_ = updates;

    last_jevt_ = jevt_;

  }

  if ( run_ != 0 &&
       evt_ != 0 &&
       status_ == "running" &&
       jevt_ - last_jevt_ > 100 ) {

    cout << "Running with no updates since too long ..." << endl;

    cout << "Forcing end-of-run ... NOW !" << endl;

    this->endRun();

    cout << "Forcing start-of-run ... NOW !" << endl;

    this->beginRun(c);

  }

//  usleep(1000);

}

void EcalBarrelMonitorClient::htmlOutput(void){

  cout << "Preparing EcalBarrelMonitorClient html output ..." << endl;

  char tmp[10];

  sprintf(tmp, "%09d", run_);

  string htmlDir = baseHtmlDir_ + "/" + tmp + "/";

  system(("/bin/mkdir -p " + htmlDir).c_str());

  ofstream htmlFile;

  htmlFile.open((htmlDir + "index.html").c_str());

  integrity_client_->htmlOutput(run_, htmlDir);
  laser_client_->htmlOutput(run_, htmlDir);
  pedestal_client_->htmlOutput(run_, htmlDir);
  pedpresample_client_->htmlOutput(run_, htmlDir);
  testpulse_client_->htmlOutput(run_, htmlDir);

  htmlFile.close();

}

