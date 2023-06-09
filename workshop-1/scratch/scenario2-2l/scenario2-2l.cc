#include <fstream>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

#include "ns3/flow-monitor-helper.h"
#include <iomanip> 

#include <ns3/ipv4-flow-classifier.h>

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

class Layer {
public:
  int nClusters;
  int nNodes;
  Layer(int clusters, int nodes){
    nClusters = clusters;
    nNodes = nodes;
  }
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
  Simulator::Schedule(Seconds (1.0), &RoutingExperiment::CheckThroughput, this);
}

static inline std::string PrintReceivedPacket (Ptr<Socket> socket, Ptr<Packet> packet, Address senderAddress) {
  std::ostringstream oss;
  oss << std::setprecision(10);

  double t2 = Simulator::Now().GetSeconds();
  //std::string diff = std::to_string(t2-t1);

  //oss << "Tiempo recibido: " << t2 << " Diferencia: " << " Id paquete: " << packet->GetUid() << " " << socket->GetNode()->GetId();
  oss << "Tiempo recibido: " << t2  << " Id paquete: " << packet->GetUid()<< " " << socket->GetNode()->GetId();
  if(InetSocketAddress::IsMatchingType(senderAddress)) {
      InetSocketAddress addr = InetSocketAddress::ConvertFrom(senderAddress);
      oss << " received one packet from " << addr.GetIpv4();
    }
  else {
      oss << " received one packet!";
    }
  return oss.str();
}

void RoutingExperiment::ReceivePacket(Ptr<Socket> socket) {
  Ptr<Packet> packet;
  Address senderAddress;
  //printf("\n%s %ld. %s %.8f\n", "Id paquete:", packet->GetUid(), "Tiempo envio: ", t1);
  while ((packet = socket->RecvFrom(senderAddress))) {
      //double t1 = Simulator::Now().GetSeconds();
      //printf("\n%s %.8f\n", "Tiempo envio: ", t1);
      bytesTotal += packet -> GetSize();
      packetsReceived += 1;
      NS_LOG_UNCOND(PrintReceivedPacket(socket, packet, senderAddress));
  }
}

Ptr<Socket> RoutingExperiment::SetupPacketReceive(Ipv4Address addr, Ptr<Node> node) {
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket(node, tid);
  InetSocketAddress local = InetSocketAddress(addr, port);
  sink -> Bind(local);
  sink -> SetRecvCallback(MakeCallback(&RoutingExperiment::ReceivePacket, this));
  int i = 0;
  printf("%d", i);
  i++;
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

  experiment.Run(nSinks, txp, CSVfileName);
}

void RoutingExperiment::Run(int nSinks, double txp, std::string CSVfileName) {
  Packet::EnablePrinting();
  m_nSinks = nSinks;
  m_txp = txp;
  m_CSVfileName = CSVfileName;

  // Parameter: number of nodes per cluster
  int nNodes = 4;
  // Parameter: number of cluster 
  int nClusters = 9;

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
  int nTotalNodes = nClusters * nNodes;

  NodeContainer layer1;
  layer1.Create(nTotalNodes);

  NodeContainer layer2;

  // Clusters
  NodeContainer cluster;
  NodeContainer cluster2;
  NodeContainer cluster3;
  NodeContainer cluster4;
  NodeContainer cluster5;
  NodeContainer cluster6;
  NodeContainer cluster7;
  NodeContainer cluster8;
  NodeContainer cluster9;

  for(int i = 0; i < nNodes; i++){
    cluster.Add(layer1.Get(i));
    cluster2.Add(layer1.Get(i+nNodes));
    cluster3.Add(layer1.Get(i+nNodes*2));
    cluster4.Add(layer1.Get(i+nNodes*3));
    cluster5.Add(layer1.Get(i+nNodes*4));
    cluster6.Add(layer1.Get(i+nNodes*5));
    cluster7.Add(layer1.Get(i+nNodes*6));
    cluster8.Add(layer1.Get(i+nNodes*7));
    cluster9.Add(layer1.Get(i+nNodes*8));
  }

  layer2.Add(cluster.Get(0));
  layer2.Add(cluster2.Get(0));
  layer2.Add(cluster3.Get(0));
  layer2.Add(cluster4.Get(0));
  layer2.Add(cluster5.Get(0));
  layer2.Add(cluster6.Get(0));
  layer2.Add(cluster7.Get(0));
  layer2.Add(cluster8.Get(0));
  layer2.Add(cluster9.Get(0));

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

  /////////////////////////////////////////////////////////////////////

  YansWifiPhyHelper wifiPhy2;
  YansWifiChannelHelper wifiChannel2;
  wifiChannel2.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel2.AddPropagationLoss("ns3::FriisPropagationLossModel");

  wifiPhy2.SetChannel(wifiChannel2.Create());

  // Add a mac and disable rate control

  wifiPhy2.Set("TxPowerStart",DoubleValue(txp));
  wifiPhy2.Set("TxPowerEnd", DoubleValue(txp));

  NetDeviceContainer netCluster = wifi.Install(wifiPhy2, wifiMac, cluster);

  /////////////////////////////////////////////////////////////////////

  YansWifiPhyHelper wifiPhy3;
  YansWifiChannelHelper wifiChannel3;
  wifiChannel3.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel3.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy3.SetChannel(wifiChannel3.Create());

  wifiPhy3.Set("TxPowerStart",DoubleValue(txp));
  wifiPhy3.Set("TxPowerEnd", DoubleValue(txp));

  NetDeviceContainer netCluster2 = wifi.Install(wifiPhy3, wifiMac, cluster2);

  /////////////////////////////////////////////////////////////////////

  YansWifiPhyHelper wifiPhy4;
  YansWifiChannelHelper wifiChannel4;
  wifiChannel4.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel4.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy4.SetChannel(wifiChannel4.Create());

  wifiPhy4.Set("TxPowerStart",DoubleValue(txp));
  wifiPhy4.Set("TxPowerEnd", DoubleValue(txp));

  NetDeviceContainer netCluster3 = wifi.Install(wifiPhy4, wifiMac, cluster3);

  /////////////////////////////////////////////////////////////////////

  YansWifiPhyHelper wifiPhy5;
  YansWifiChannelHelper wifiChannel5;
  wifiChannel5.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel5.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy5.SetChannel(wifiChannel5.Create());

  wifiPhy5.Set("TxPowerStart",DoubleValue(txp));
  wifiPhy5.Set("TxPowerEnd", DoubleValue(txp));

  NetDeviceContainer netCluster4 = wifi.Install(wifiPhy5, wifiMac, cluster4);

  /////////////////////////////////////////////////////////////////////

  YansWifiPhyHelper wifiPhy6;
  YansWifiChannelHelper wifiChannel6;
  wifiChannel6.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel6.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy6.SetChannel(wifiChannel6.Create());

  wifiPhy6.Set("TxPowerStart",DoubleValue(txp));
  wifiPhy6.Set("TxPowerEnd", DoubleValue(txp));

  NetDeviceContainer netCluster5 = wifi.Install(wifiPhy6, wifiMac, cluster5);
  /////////////////////////////////////////////////////////////////////

  YansWifiPhyHelper wifiPhy7;
  YansWifiChannelHelper wifiChannel7;
  wifiChannel7.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel7.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy7.SetChannel(wifiChannel7.Create());

  wifiPhy7.Set("TxPowerStart",DoubleValue(txp));
  wifiPhy7.Set("TxPowerEnd", DoubleValue(txp));

  NetDeviceContainer netCluster6 = wifi.Install(wifiPhy7, wifiMac, cluster6);
  /////////////////////////////////////////////////////////////////////

  YansWifiPhyHelper wifiPhy8;
  YansWifiChannelHelper wifiChannel8;
  wifiChannel8.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel8.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy8.SetChannel(wifiChannel8.Create());

  wifiPhy8.Set("TxPowerStart",DoubleValue(txp));
  wifiPhy8.Set("TxPowerEnd", DoubleValue(txp));

  NetDeviceContainer netCluster7 = wifi.Install(wifiPhy8, wifiMac, cluster7);
  /////////////////////////////////////////////////////////////////////

  YansWifiPhyHelper wifiPhy9;
  YansWifiChannelHelper wifiChannel9;
  wifiChannel9.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel9.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy9.SetChannel(wifiChannel9.Create());

  wifiPhy9.Set("TxPowerStart",DoubleValue(txp));
  wifiPhy9.Set("TxPowerEnd", DoubleValue(txp));

  NetDeviceContainer netCluster8 = wifi.Install(wifiPhy9, wifiMac, cluster8);
  /////////////////////////////////////////////////////////////////////

  YansWifiPhyHelper wifiPhy10;
  YansWifiChannelHelper wifiChannel10;
  wifiChannel10.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel10.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy10.SetChannel(wifiChannel10.Create());

  wifiPhy10.Set("TxPowerStart",DoubleValue(txp));
  wifiPhy10.Set("TxPowerEnd", DoubleValue(txp));

  NetDeviceContainer netCluster9 = wifi.Install(wifiPhy10, wifiMac, cluster9);
  /////////////////////////////////////////////////////////////////////

  YansWifiPhyHelper wifiPhy11;
  YansWifiChannelHelper wifiChannel11;
  wifiChannel11.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel11.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy11.SetChannel(wifiChannel11.Create());

  wifiPhy11.Set("TxPowerStart",DoubleValue(txp));
  wifiPhy11.Set("TxPowerEnd", DoubleValue(txp));

  NetDeviceContainer nLayer2 = wifi.Install(wifiPhy11, wifiMac, layer2);

  //////////////////////////////////// END /////////////////////////////////////////

  MobilityHelper mobilityAdhoc;

  int64_t streamIndex = 0; // used to get consistent mobility across scenarios

  ObjectFactory pos;
  // Parameter: Geographical space 
  pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
  pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
  pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
  //pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=40.0]"));
  //pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=40.0]"));

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
  mobilityAdhoc.Install(cluster);
  mobilityAdhoc.Install(cluster2);
  mobilityAdhoc.Install(cluster3);
  mobilityAdhoc.Install(cluster4);
  mobilityAdhoc.Install(cluster5);
  mobilityAdhoc.Install(cluster6);
  mobilityAdhoc.Install(cluster7);
  mobilityAdhoc.Install(cluster8);
  mobilityAdhoc.Install(cluster9);
/*
  // Parameter: Geographical space 
  pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
  pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=120.0|Max=150.0]"));
  pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=120.0|Max=150.0]"));

  taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
  streamIndex += taPositionAlloc->AssignStreams (streamIndex);

  mobilityAdhoc.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue (ssSpeed.str ()),
                                  "Pause", StringValue (ssPause.str ()),
                                  "PositionAllocator", PointerValue (taPositionAlloc));
  mobilityAdhoc.SetPositionAllocator(taPositionAlloc);
  mobilityAdhoc.Install(cluster2);
*/
  streamIndex += mobilityAdhoc.AssignStreams (cluster, streamIndex);

  NS_UNUSED (streamIndex); // From this point, streamIndex is unused

  OlsrHelper olsr;
  Ipv4ListRoutingHelper list;
  InternetStackHelper internet;
  list.Add (olsr, 100);
  m_protocolName = "OLSR";

  internet.SetRoutingHelper(list);
  internet.Install(layer1);

  NS_LOG_INFO ("assigning ip address");

  // SETTING NETWORK

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer layer1I = ipv4.Assign(nLayer1);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer clusterI = ipv4.Assign(netCluster);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer cluster2I = ipv4.Assign(netCluster2);

  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer cluster3I = ipv4.Assign(netCluster3);

  ipv4.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer cluster4I = ipv4.Assign(netCluster4);

  ipv4.SetBase("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer cluster5I = ipv4.Assign(netCluster5);

  ipv4.SetBase("10.1.7.0", "255.255.255.0");
  Ipv4InterfaceContainer cluster6I = ipv4.Assign(netCluster6);

  ipv4.SetBase("10.1.8.0", "255.255.255.0");
  Ipv4InterfaceContainer cluster7I = ipv4.Assign(netCluster7);

  ipv4.SetBase("10.1.9.0", "255.255.255.0");
  Ipv4InterfaceContainer cluster8I = ipv4.Assign(netCluster8);

  ipv4.SetBase("10.1.10.0", "255.255.255.0");
  Ipv4InterfaceContainer cluster9I = ipv4.Assign(netCluster9);

  ipv4.SetBase("10.1.11.0", "255.255.255.0");
  Ipv4InterfaceContainer layer2I = ipv4.Assign(nLayer2);

  //OnOffHelper onoff1 ("ns3::UdpSocketFactory", Address());
  OnOffHelper onoff1 ("ns3::UdpSocketFactory", InetSocketAddress(layer2I.GetAddress(0), port));

  // Poisson ??
  onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));

  // Sending Pack
  for(int i = 0; i < 1; i++){

    ////// RECEIVER 
    Ptr<Socket> sink = SetupPacketReceive(layer2I.GetAddress(0), layer2.Get(0));

    ////// SENDER
    Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
    ApplicationContainer temp = onoff1.Install(layer1.Get(31));
    double t = var->GetValue(10.0, 11.0);

    temp.Start(Seconds(t));
    //temp.Start (Seconds(var->GetValue(100.0, 101.0)));
    temp.Stop(Seconds(TotalTime));
  }

  std::stringstream ss;
  ss << nNodes;
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
  MobilityHelper::EnableAsciiAll(ascii.CreateFileStream(tr_name + ".mob"));

  //Ptr<FlowMonitor> flowmon;
  FlowMonitorHelper flowmon;
  //flowmon = flowmonHelper.InstallAll();
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  //FlowMonitor fm = flowmon->GetMonitor();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  NS_LOG_INFO ("Run Simulation.");

  CheckThroughput();

  Simulator::Stop(Seconds(TotalTime));
  Simulator::Run();

  NS_LOG_UNCOND("Checking for lost packets...");

  monitor->CheckForLostPackets ();

  NS_LOG_UNCOND("Checking flows...");
  
  for(std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
      
      if(t.sourcePort == port || t.destinationPort == port) {
        NS_LOG_UNCOND("Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Protocol " << t.protocol);
        NS_LOG_UNCOND("Tx Packets = " << iter->second.txPackets);
        NS_LOG_UNCOND("Rx Packets = " << iter->second.rxPackets);
        NS_LOG_UNCOND("Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds()) / 1024  << " Kbps");
      }
    }

  monitor->SerializeToXmlFile((tr_name + ".flowmon").c_str(), false, false);

  Simulator::Destroy();
}

