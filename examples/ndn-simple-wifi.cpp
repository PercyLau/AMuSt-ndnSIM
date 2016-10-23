#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ndnSIM-module.h"

//extra header needed for app
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM/apps/ndn-app.hpp"

using namespace std;
namespace ns3 {

NS_LOG_COMPONENT_DEFINE("ndn.WiFiExample");

int
main(int argc, char* argv[])
{
  // the below configuration disable fragmentation. !WARNING! It is not compatible with DASH
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("1300"));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("1300"));
  Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("OfdmRate24Mbps"));
  //Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("24Mbps"));
  //Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  //Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("20"));

  //Turn off RTSCTS if packet is less than 2200 byte
  //Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
  CommandLine cmd;
  cmd.Parse(argc, argv);

  //////////////////////
  //////////////////////
  //////////////////////
  WifiHelper wifi = WifiHelper::Default();
  // wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode",
                               StringValue("OfdmRate24Mbps"));

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
  //wifiMacHelper.SetType("ns3::WifiNetDevice","Mtu",1500);

  Ptr<UniformRandomVariable> randomizer = CreateObject<UniformRandomVariable>();
  randomizer->SetAttribute("Min", DoubleValue(10)); // 
  randomizer->SetAttribute("Max", DoubleValue(30));

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


  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue (Rectangle(0,40,0,40)),"Distance",DoubleValue(4),"Speed",PointerValue(randomizer));

  NodeContainer nodes;
  nodes.Create(9);

  ////////////////
  // 1. Install Wifi
  NetDeviceContainer wifiNetDevices = wifi.Install(wifiPhyHelper, wifiMacHelper, nodes);
  //wifiNetDevices
  // 2. Install Mobility model
  mobility.Install(nodes);

  // 3. TODO
  

  // 4. Install NDN stack
  NS_LOG_INFO("Installing NDN stack");
  ndn::StackHelper ndnHelper;
  // ndnHelper.AddNetDeviceFaceCreateCallback (WifiNetDevice::GetTypeId (), MakeCallback
  // (MyNetDeviceFaceCallback));
  ndnHelper.SetOldContentStore("ns3::ndn::cs::Lru", "MaxSize", "100");
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.Install(nodes);

  // Set BestRoute strategy
  //ndn::StrategyChoiceHelper::Install(nodes, "/", "ndn:/localhost/nfd/strategy/client-control");
  ndn::StrategyChoiceHelper::Install(nodes, "/myprefix", "ndn:/localhost/nfd/strategy/client-control");

  // 5. Set up applications
  NS_LOG_INFO("Installing Applications");

  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  //ndn::AppHelper consumerHelper("ns3::ndn::FileConsumerCbr");
  //consumerHelper.SetAttribute("FileToRequest", StringValue("/myprefix/file1.img"));

  consumerHelper.SetAttribute("Frequency", DoubleValue(24.0));
  consumerHelper.Install(nodes.Get(1));


  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  producerHelper.SetPrefix("/myprefix");

  //DASH parameters
  //ndn::AppHelper producerHelper("ns3::ndn::FileServer");
  //producerHelper.SetAttribute("ContentDirectory", StringValue("/home/someuser/somedata"));

  //ndnGlobalRoutingHelper.AddOrigins("/myprefix",nodes,Get(6));
  producerHelper.Install(nodes.Get(8));

  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll();
  ndnGlobalRoutingHelper.AddOrigins("/myprefix",nodes.Get(8));
  ndn::GlobalRoutingHelper::CalculateRoutes();

  ////////////////
  NS_LOG_UNCOND("Simulation Finished.");
  Simulator::Stop(Seconds(30.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}