/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2015 Christian Kreuzberger and Daniel Posch, Alpen-Adria-University 
 * Klagenfurt
 *
 * This file is part of amus-ndnSIM, based on ndnSIM. See AUTHORS for complete list of 
 * authors and contributors.
 *
 * amus-ndnSIM and ndnSIM are free software: you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by the Free Software 
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * amus-ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * amus-ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/ndnSIM/apps/ndn-app.hpp"

namespace ns3 {

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
  // setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("10Mbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("100ms"));
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("20"));

  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Creating nodes
  NodeContainer nodes;
  nodes.Create(3); // 3 nodes, connected: 0 <---> 1 <---> 2

  // Connecting nodes using two links
  PointToPointHelper p2p;
  p2p.Install(nodes.Get(0), nodes.Get(1));
  p2p.Install(nodes.Get(1), nodes.Get(2));

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.setCsSize(10);
  ndnHelper.setOpMIPS(1000);
  ndnHelper.SetOldContentStore("ns3::ndn::cs::Lru", "MaxSize", "100");
  ndnHelper.InstallAll();

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll("/myprefix", "/localhost/nfd/strategy/best-route");

  ns3::ndn::AppHelper consumerHelper("ns3::ndn::FileConsumerCbr::MultimediaConsumer");
  consumerHelper.SetAttribute("AllowUpscale", BooleanValue(true));
  consumerHelper.SetAttribute("AllowDownscale", BooleanValue(true));
  consumerHelper.SetAttribute("ScreenWidth", UintegerValue(1920));
  consumerHelper.SetAttribute("ScreenHeight", UintegerValue(1080));
  consumerHelper.SetAttribute("StartRepresentationId", StringValue("auto"));
  consumerHelper.SetAttribute("MaxBufferedSeconds", UintegerValue(30));
  consumerHelper.SetAttribute("StartUpDelay", StringValue("1"));

  consumerHelper.SetAttribute("AdaptationLogic", StringValue("dash::player::RateAndBufferBasedAdaptationLogic"));
  consumerHelper.SetAttribute("MpdFileToRequest", StringValue(std::string("/home/percy/multimediaData/AVC/BBB-2s.mpd" )));

  //consumerHelper.SetPrefix (std::string("/Server_" + boost::lexical_cast<std::string>(i%server.size ()) + "/layer0"));
  ApplicationContainer app1 = consumerHelper.Install (nodes.Get(2));

    // Connect Tracers
  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/FileDownloadFinished", MakeCallback(&FileDownloadedTrace));
  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/ManifestReceived", MakeCallback(&FileDownloadedManifestTrace));
  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/FileDownloadStarted", MakeCallback(&FileDownloadStartedTrace));

  // Producer
  ndn::AppHelper producerHelper("ns3::ndn::FileServer");

  // Producer will reply to all requests starting with /myprefix
  producerHelper.SetPrefix("/home/percy/multimediaData/AVC");
  producerHelper.SetAttribute("ContentDirectory", StringValue("/home/percy/multimediaData/AVC"));
  producerHelper.Install(nodes.Get(0)); // install to some node from nodelist

  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll();

  ndnGlobalRoutingHelper.AddOrigins("/home/percy/multimediaData/", nodes.Get(0));
  ndn::GlobalRoutingHelper::CalculateRoutes();

  Simulator::Stop(Seconds(1200.0));

  //ndn::DASHPlayerTracer::InstallAll("dash-output-ndn.txt");
  //ndn::CsTracer::InstallAll("cs-trace-ndn.txt", Seconds(1));
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



