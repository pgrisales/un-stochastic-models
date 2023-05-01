#include <fstream>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("manet-routing-compare");

class RoutingExperiment {
public:
  RoutingExperiment ();
  void Run (int nSinks, double txp, std::string CSVfileName);
  std::string CommandSetup (int argc, char **argv);

private:
  Ptr<Socket> SetupPacketReceive (Ipv4Address addr, Ptr<Node> node);
  void ReceivePacket(Ptr<Socket> socket);
  void CheckThroughput();

  uint32_t port;
  uint32_t bytesTotal;
  uint32_t packetsReceived;

  std::string m_CSVfileName;
  int m_nSinks;
  std::string m_protocolName;
  double m_txp;
  bool m_traceMobility;
  uint32_t m_protocol;
};

class layer {
public:
  RoutingExperiment experiment;
};

RoutingExperiment::RoutingExperiment ()
  : port (9),
    bytesTotal (0),
    packetsReceived (0),
    m_CSVfileName ("manet-routing.output.csv"),
    m_traceMobility (false),
    m_protocol(1) // 1=OLSR, 2=AODV
{
}

void RoutingExperiment::CheckThroughput () {
  double kbs = (bytesTotal * 8.0) / 1000;
  bytesTotal = 0;

  std::ofstream out (m_CSVfileName.c_str (), std::ios::app);

  out << (Simulator::Now()).GetSeconds () << ","
      << kbs << ","
      << packetsReceived << ","
      << m_nSinks << ","
      << m_protocolName << ","
      << m_txp << ""
      << std::endl;

  out.close ();
  packetsReceived = 0;
  Simulator::Schedule (Seconds (1.0), &RoutingExperiment::CheckThroughput, this);
}

static inline std::string PrintReceivedPacket (Ptr<Socket> socket, Ptr<Packet> packet, Address senderAddress) {
  std::ostringstream oss;

  oss << Simulator::Now().GetSeconds() << " " << socket->GetNode()->GetId();
  //int idx = socket -> GetNode() -> GetId();
  //InetSocketAddress receiver = InetSocketAddress::ConvertFrom(socket->GetNode()->GetDevice(idx)->GetAddress());
  //InetSocketAddress receiver = InetSocketAddress::ConvertFrom(socket->RecvFrom());
  //oss << Simulator::Now().GetSeconds() << " " << receiver.GetIpv4();

  if(InetSocketAddress::IsMatchingType(senderAddress)) {
      InetSocketAddress addr = InetSocketAddress::ConvertFrom(senderAddress);
      oss << " received one packet from " << addr.GetIpv4();
    }
  else {
      oss << " received one packet!";
    }
  return oss.str ();
}

void RoutingExperiment::ReceivePacket (Ptr<Socket> socket) {
  Ptr<Packet> packet;
  Address senderAddress;
  while ((packet = socket -> RecvFrom(senderAddress))) {
      bytesTotal += packet -> GetSize();
      packetsReceived += 1;
      NS_LOG_UNCOND (PrintReceivedPacket(socket, packet, senderAddress));
    }
}

Ptr<Socket> RoutingExperiment::SetupPacketReceive(Ipv4Address addr, Ptr<Node> node) {
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket(node, tid);
  InetSocketAddress local = InetSocketAddress(addr, port);
  sink -> Bind(local);
  sink -> SetRecvCallback(MakeCallback(&RoutingExperiment::ReceivePacket, this));

  return sink;
}

std::string RoutingExperiment::CommandSetup (int argc, char **argv) {
  CommandLine cmd(__FILE__);
  cmd.AddValue ("CSVfileName", "The name of the CSV output file name", m_CSVfileName);
  cmd.AddValue ("traceMobility", "Enable mobility tracing", m_traceMobility);
  cmd.AddValue ("protocol", "1=OLSR;2=AODV;3=DSDV;4=DSR", m_protocol);
  cmd.Parse (argc, argv);
  return m_CSVfileName;
}

int main (int argc, char *argv[]) {
  RoutingExperiment experiment;
  std::string CSVfileName = experiment.CommandSetup(argc,argv);

  //blank out the last output file and write the column headers
  std::ofstream out (CSVfileName.c_str());
  out << "SimulationSecond," <<
  "ReceiveRate," <<
  "PacketsReceived," <<
  "NumberOfSinks," <<
  "RoutingProtocol," <<
  "TransmissionPower" <<
  std::endl;
  out.close ();

  int nSinks = 3;
  double txp = 7.5;

  experiment.Run (nSinks, txp, CSVfileName);
}

void RoutingExperiment::Run (int nSinks, double txp, std::string CSVfileName) {
  Packet::EnablePrinting();
  m_nSinks = nSinks;
  m_txp = txp;
  m_CSVfileName = CSVfileName;

  // Parameter: number of nodes per cluster
  int nWifis = 12;
  // Parameter: number of cluster 
  int nCluster = 2;

  // number repetitions
  double TotalTime = 200.0;
  std::string rate("2048bps");
  std::string phyMode("DsssRate11Mbps");
  std::string tr_name("manet-routing-compare");
  int nodeSpeed = 20; //in m/s
  int nodePause = 0; //in s
  m_protocolName = "protocol";

  Config::SetDefault  ("ns3::OnOffApplication::PacketSize",StringValue ("64"));
  Config::SetDefault ("ns3::OnOffApplication::DataRate",  StringValue (rate));

  //Set Non-unicastMode rate to unicast mode
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",StringValue (phyMode));

  // Layer 
  int nTotalNodes = nCluster * nWifis;

  NodeContainer layer1;
  layer1.Create(nTotalNodes);

  NodeContainer layer2;

  // Clusters
  NodeContainer adhocNodes;
  NodeContainer adhocNodes2;

  for(int i = 0; i < nWifis; i++){
    adhocNodes.Add(layer1.Get(i));
  }

  layer2.Add(adhocNodes.Get(0));

  for(int i = 0; i < nWifis; i++){
    adhocNodes2.Add(layer1.Get(i+nWifis));
  }

  layer2.Add(adhocNodes2.Get(0));

  //////////////////////////////////// START /////////////////////////////////////////

  //////// Setting up wifi phy and channel using helpers
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel(wifiChannel.Create ());

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));

  // Add a mac and disable rate control
  WifiMacHelper wifiMac;

  wifiPhy.Set("TxPowerStart",DoubleValue(txp));
  wifiPhy.Set("TxPowerEnd", DoubleValue(txp));

  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer nLayer1 = wifi.Install(wifiPhy, wifiMac, layer1);

  ////////// Setting up wifi phy and channel using helpers 2

  YansWifiPhyHelper wifiPhy2;
  YansWifiChannelHelper wifiChannel2;
  wifiChannel2.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel2.AddPropagationLoss("ns3::FriisPropagationLossModel");

  wifiPhy2.SetChannel(wifiChannel2.Create());

  // Add a mac and disable rate control

  wifiPhy2.Set("TxPowerStart",DoubleValue(txp));
  wifiPhy2.Set("TxPowerEnd", DoubleValue(txp));

  NetDeviceContainer adhocDevices = wifi.Install(wifiPhy2, wifiMac, adhocNodes);

  ///////// Setting up wifi phy and channel using helpers 3

  YansWifiPhyHelper wifiPhy3;
  YansWifiChannelHelper wifiChannel3;
  wifiChannel3.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel3.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy3.SetChannel(wifiChannel3.Create());

  wifiPhy3.Set("TxPowerStart",DoubleValue(txp));
  wifiPhy3.Set("TxPowerEnd", DoubleValue(txp));

  NetDeviceContainer adhocDevices2 = wifi.Install(wifiPhy3, wifiMac, adhocNodes2);
  ///////// Setting up wifi phy and channel using helpers 3

  YansWifiPhyHelper wifiPhy4;
  YansWifiChannelHelper wifiChannel4;
  wifiChannel4.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel4.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy4.SetChannel(wifiChannel4.Create());

  wifiPhy4.Set("TxPowerStart",DoubleValue(txp));
  wifiPhy4.Set("TxPowerEnd", DoubleValue(txp));

  NetDeviceContainer nLayer2 = wifi.Install(wifiPhy4, wifiMac, layer2);

  //////////////////////////////////// END /////////////////////////////////////////

  MobilityHelper mobilityAdhoc;

  int64_t streamIndex = 0; // used to get consistent mobility across scenarios

  ObjectFactory pos;
  // Parameter: Geographical space 
  pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
  pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=40.0]"));
  pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=40.0]"));

  Ptr<PositionAllocator> taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
  streamIndex += taPositionAlloc->AssignStreams (streamIndex);

  std::stringstream ssSpeed;
  ssSpeed << "ns3::UniformRandomVariable[Min=0.0|Max=" << nodeSpeed << "]";
  std::stringstream ssPause;
  ssPause << "ns3::ConstantRandomVariable[Constant=" << nodePause << "]";
  mobilityAdhoc.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue (ssSpeed.str ()),
                                  "Pause", StringValue (ssPause.str ()),
                                  "PositionAllocator", PointerValue (taPositionAlloc));
  mobilityAdhoc.SetPositionAllocator(taPositionAlloc);
  mobilityAdhoc.Install(adhocNodes);

  // Parameter: Geographical space 
  pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
  pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=120.0|Max=150.0]"));
  pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=120.0|Max=150.0]"));

  taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
  streamIndex += taPositionAlloc->AssignStreams (streamIndex);

  ssSpeed << "ns3::UniformRandomVariable[Min=0.0|Max=" << nodeSpeed << "]";
  ssPause << "ns3::ConstantRandomVariable[Constant=" << nodePause << "]";
  mobilityAdhoc.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue (ssSpeed.str ()),
                                  "Pause", StringValue (ssPause.str ()),
                                  "PositionAllocator", PointerValue (taPositionAlloc));
  mobilityAdhoc.SetPositionAllocator(taPositionAlloc);
  mobilityAdhoc.Install(adhocNodes2);

  streamIndex += mobilityAdhoc.AssignStreams (adhocNodes, streamIndex);

  NS_UNUSED (streamIndex); // From this point, streamIndex is unused

  AodvHelper aodv;
  OlsrHelper olsr;
  Ipv4ListRoutingHelper list;
  InternetStackHelper internet;

  switch (m_protocol) {
    case 1:
      list.Add (olsr, 100);
      m_protocolName = "OLSR";
      break;
    case 2:
      list.Add (aodv, 100);
      m_protocolName = "AODV";
      break;
  }

  internet.SetRoutingHelper(list);
  internet.Install(layer1);

  NS_LOG_INFO ("assigning ip address");

  // SETTING NETWORK

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer layer1Interfaces = ipv4.Assign(nLayer1);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer adhocInterfaces = ipv4.Assign(adhocDevices);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer adhocInterfaces2 = ipv4.Assign(adhocDevices2);

  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer layer2Interfaces = ipv4.Assign(nLayer2);

  //OnOffHelper onoff1 ("ns3::UdpSocketFactory", Address());
  OnOffHelper onoff1 ("ns3::UdpSocketFactory", InetSocketAddress(layer2Interfaces.GetAddress(0), port));

  // Poisson ??
  onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));

  // Sending Pack

  for(int i = 0; i < 1; i++){

    ////// RECEIVER 

    //Ptr<Socket> sink = SetupPacketReceive(adhocInterfaces.GetAddress(0), adhocNodes.Get(0));
    //AddressValue remoteAddress(InetSocketAddress(adhocInterfaces.GetAddress(0), port));
    Ptr<Socket> sink = SetupPacketReceive(layer2Interfaces.GetAddress(0), layer2.Get(0));
    //AddressValue remoteAddress(InetSocketAddress(layer2Interfaces.GetAddress(0), port));

    //AddressValue remoteAddress2(InetSocketAddress(adhocInterfaces2.GetAddress(0), port));

    //onoff1.SetAttribute ("Remote", remoteAddress);
    //onoff1.SetAttribute ("Remote", remoteAddress2);

    ////// SENDER

    Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
    //ApplicationContainer temp = onoff1.Install(adhocNodes.Get(1));
    //ApplicationContainer temp = onoff1.Install(adhocNodes2.Get(0));
    //ApplicationContainer temp = onoff1.Install(layer1.Get(12));
    ApplicationContainer temp = onoff1.Install(layer1.Get(12));
    temp.Start (Seconds (var -> GetValue (100.0, 101.0)));
    temp.Stop (Seconds (TotalTime));
  }

  std::stringstream ss;
  ss << nWifis;
  std::string nodes = ss.str();

  std::stringstream ss2;
  ss2 << nodeSpeed;
  std::string sNodeSpeed = ss2.str();

  std::stringstream ss3;
  ss3 << nodePause;
  std::string sNodePause = ss3.str();

  std::stringstream ss4;
  ss4 << rate;
  std::string sRate = ss4.str();

  AsciiTraceHelper ascii;
  MobilityHelper::EnableAsciiAll(ascii.CreateFileStream (tr_name + ".mob"));

  //Ptr<FlowMonitor> flowmon;
  //FlowMonitorHelper flowmonHelper;
  //flowmon = flowmonHelper.InstallAll ();

  NS_LOG_INFO ("Run Simulation.");

  CheckThroughput();

  Simulator::Stop(Seconds (TotalTime));
  Simulator::Run();

  //flowmon->SerializeToXmlFile ((tr_name + ".flowmon").c_str(), false, false);

  Simulator::Destroy();
}

