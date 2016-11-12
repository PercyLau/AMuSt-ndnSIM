#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ndnSIM/apps/ndn-app.hpp"
#include "ns3/ndnSIM-module.h"

using namespace std;
namespace ns3 {


NS_LOG_COMPONENT_DEFINE("ndn.AVC.WiFiExample");

void
FileDownloadedTrace(Ptr<ns3::ndn::App> app, shared_ptr<const ndn::Name> interestName, double downloadSpeed, long milliSeconds)
{
  std::cout << "Trace: File finished downloading: " << Simulator::Now().GetMilliSeconds () << " "<< *interestName <<
     " Download Speed: " << downloadSpeed/1000.0 << " Kilobit/s in " << milliSeconds << " ms" << std::endl;
}

void
FileDownloadedManifestTrace(Ptr<ns3::ndn::App> app, shared_ptr<const ndn::Name> interestName, long fileSize)
{
  std::cout << "Trace: Manifest received: " << Simulator::Now().GetMilliSeconds () <<" "<< *interestName << " File Size: " << fileSize << std::endl;
}

void
FileDownloadStartedTrace(Ptr<ns3::ndn::App> app, shared_ptr<const ndn::Name> interestName)
{
  std::cout << "Trace: File started downloading: " << Simulator::Now().GetMilliSeconds () <<" "<< *interestName << std::endl;
}

int
main(int argc, char* argv[])
{
  // disable fragmentation
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode",
                     StringValue("OfdmRate24Mbps"));

  CommandLine cmd;
  cmd.Parse(argc, argv);

  //////////////////////
  //////////////////////
  //////////////////////
  WifiHelper wifi = WifiHelper::Default();
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
  //wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate24Mbps"));

  YansWifiChannelHelper wifiChannel; // = YansWifiChannelHelper::Default ();
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::ThreeLogDistancePropagationLossModel");
  wifiChannel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");

  // YansWifiPhy wifiPhy = YansWifiPhy::Default();
  YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default();
  wifiPhyHelper.SetChannel(wifiChannel.Create());
  wifiPhyHelper.Set("TxPowerStart", DoubleValue(2)); //power start and end must be the same
  wifiPhyHelper.Set("TxPowerEnd", DoubleValue(2));

  NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default();
  wifiMacHelper.SetType("ns3::AdhocWifiMac");

  Ptr<UniformRandomVariable> randomizer = CreateObject<UniformRandomVariable>();
  randomizer->SetAttribute("Min", DoubleValue(1)); // 
  randomizer->SetAttribute("Max", DoubleValue(10));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator", "X", PointerValue(randomizer),
                                "Y", PointerValue(randomizer), "Z", PointerValue(randomizer));
  //mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  //mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
  //                               "MinX", DoubleValue (0.0),
  //                               "MinY", DoubleValue (50.0),
  //                               "DeltaX", DoubleValue (10.0),
  //                               "DeltaY", DoubleValue (10.0),
  //                               "GridWidth", UintegerValue (10),
  //                              "LayoutType", StringValue ("RowFirst"));


  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue (Rectangle(0,100,0,100)),"Distance",DoubleValue(4),"Speed",PointerValue(randomizer));

  NodeContainer nodes;
  nodes.Create(4);

  ////////////////
  // 1. Install Wifi
  NetDeviceContainer wifiNetDevices = wifi.Install(wifiPhyHelper, wifiMacHelper, nodes);

  // 2. Install Mobility model
  mobility.Install(nodes);

  // 3. Install NDN stack
  NS_LOG_INFO("Installing NDN stack");
  ndn::StackHelper ndnHelper;
  // ndnHelper.AddNetDeviceFaceCreateCallback (WifiNetDevice::GetTypeId (), MakeCallback
  // (MyNetDeviceFaceCallback));
  //ndnHelper.SetOldContentStore("ns3::ndn::cs::Lru", "MaxSize", "1");
  ndnHelper.setCsSize(10);
  ndnHelper.setOpMIPS(1);
  ndnHelper.SetDefaultRoutes(true);
  
  ndnHelper.Install(nodes.Get(1));
  ndnHelper.Install(nodes.Get(2));
  

  //ndnHelper.SetOldContentStore("ns3::ndn::cs::Lru", "MaxSize", "1");
  //ndnHelper.setCsSize(100);
  //ndnHelper.setOpMIPS(10000000);
  //ndnHelper.Install(nodes.Get(3));
  //ndnHelper.Install(nodes.Get(4));
  //ndnHelper.Install(nodes.Get(5));
  //ndnHelper.Install(nodes.Get(6));
  //ndnHelper.Install(nodes.Get(7));
  
  ndnHelper.setCsSize(10);
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.setOpMIPS(1000);
  ndnHelper.Install(nodes.Get(0));
  
  ndnHelper.setOpMIPS(1);
  ndnHelper.Install(nodes.Get(3));
  // Set routing strategy
  ndn::StrategyChoiceHelper::Install(nodes, "/", "ndn:/localhost/nfd/strategy/oon");

  // 4. Set up client devices
  NS_LOG_INFO("Installing Applications");

  //set up mobile video consumer
  ns3::ndn::AppHelper consumerHelper("ns3::ndn::FileConsumerCbr::MultimediaConsumer");
  consumerHelper.SetAttribute("AllowUpscale", BooleanValue(true));
  consumerHelper.SetAttribute("AllowDownscale", BooleanValue(true));
  consumerHelper.SetAttribute("ScreenWidth", UintegerValue(1920));
  consumerHelper.SetAttribute("ScreenHeight", UintegerValue(1080));
  consumerHelper.SetAttribute("StartRepresentationId", StringValue("auto"));
  consumerHelper.SetAttribute("MaxBufferedSeconds", UintegerValue(10));
  consumerHelper.SetAttribute("StartUpDelay", StringValue("0.5"));
  
  consumerHelper.SetAttribute("AdaptationLogic", StringValue("dash::player::RateBasedAdaptationLogic"));
  consumerHelper.SetAttribute("MpdFileToRequest", StringValue(std::string("/home/lockheed/multimediaData/AVC/BBB-2s.mpd" )));

  ApplicationContainer consumer_0 = consumerHelper.Install(nodes.Get(0));
  consumer_0.Start(Seconds(0.5)); //precache
  consumerHelper.SetAttribute("MpdFileToRequest", StringValue(std::string("/home/lockheed/multimediaData/AVC/BBB-2s-v1.mpd" )));
  ApplicationContainer consumer_1 = consumerHelper.Install(nodes.Get(1));
  consumer_1.Start(Seconds(1000));
  ApplicationContainer consumer_2 = consumerHelper.Install(nodes.Get(2));
  consumer_2.Start(Seconds(1000));
  //consumerHelper.Install(nodes.Get(3));
  //consumerHelper.Install(nodes.Get(4));

      // Connect Tracers
  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/FileDownloadFinished",
                               MakeCallback(&FileDownloadedTrace));
  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/ManifestReceived",
                               MakeCallback(&FileDownloadedManifestTrace));
  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/FileDownloadStarted",
                               MakeCallback(&FileDownloadStartedTrace));

  // 5. Set up server devices
  ndn::AppHelper mpdProducerHelper("ns3::ndn::FileServer");
  mpdProducerHelper.SetPrefix("/home/lockheed/multimediaData/AVC/");
  mpdProducerHelper.SetAttribute("ContentDirectory", StringValue("/home/lockheed/multimediaData/AVC/"));
  mpdProducerHelper.Install(nodes.Get(3));
  //mpdProducerHelper.Install(nodes.Get(0));
  // 6. Set global routing?
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll();
  ndnGlobalRoutingHelper.AddOrigins("/home/lockheed/multimediaData/",nodes.Get(3));
  //ndnGlobalRoutingHelper.AddOrigins("/home/lockheed/multimediaData/",nodes.Get(0));
  ndn::GlobalRoutingHelper::CalculateRoutes();

  //producerHelper.Install(nodes.Get(6));
  //producerHelper.SetPrefix("/node1/prefix");
  //producerHelper.SetAttribute("PayloadSize", StringValue("1500"));
  //producerHelper.Install(nodes.Get(7));
  //producerHelper.SetPrefix("/");
  //producerHelper.SetAttribute("PayloadSize", StringValue("1500"));
  //producerHelper.Install(nodes.Get(8));
  ////////////////

  Simulator::Stop(Seconds(4000));
  //ndn::DASHPlayerTracer::InstallAll("dash-output-oon.txt");
  //ndn::L3RateTracer::InstallAll("rate-trace-oon.txt", Seconds(0.5));
  //L2RateTracer::InstallAll("L2-output-oon.txt",Seconds(0.5));
  //ndn::AppDelayTracer::InstallAll("app-delays-trace-oon.txt");
  //ndn::FileConsumerLogTracer::InstallAll("file-consumer-log-trace.txt");

  Simulator::Run();
  Simulator::Destroy();

  NS_LOG_UNCOND("Simulation Finished.");
  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}