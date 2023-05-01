#include <fstream>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("manet-routing-compare");

class RoutingExperiment {
  public:
    RoutingExperiment ();
    void Run (int nSinks, double txp, int nWifis);
    std::string CommandSetup (int argc, char **argv);

  private:
    Ptr<Socket> SetupPacketReceive (Ipv4Address addr, Ptr<Node> node);
    void ReceivePacket (Ptr<Socket> socket);

    uint32_t port;
    uint32_t bytesTotal;
    uint32_t packetsReceived;

    std::string m_protocolName;
    bool m_traceMobility;
};
RoutingExperiment::RoutingExperiment ()
  : port (9),
    bytesTotal (0),
    packetsReceived (0),
    m_traceMobility (false)
{
}

Ptr<Socket> RoutingExperiment::SetupPacketReceive (Ipv4Address addr, Ptr<Node> node) {
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket (node, tid);
  InetSocketAddress local = InetSocketAddress (addr, port);
  sink->Bind (local);
  sink->SetRecvCallback (MakeCallback (&RoutingExperiment::ReceivePacket, this));

  return sink;
}

static inline std::string PrintReceivedPacket (Ptr<Socket> socket, Ptr<Packet> packet, Address senderAddress) {
  std::ostringstream oss;

  oss << Simulator::Now ().GetSeconds () << " " << socket->GetNode ()->GetId ();

  if (InetSocketAddress::IsMatchingType (senderAddress)) {
      InetSocketAddress addr = InetSocketAddress::ConvertFrom (senderAddress);
      oss << " received one packet from " << addr.GetIpv4 ();
    }
  else {
      oss << " received one packet!";
    }
  return oss.str ();
}

void RoutingExperiment::ReceivePacket (Ptr<Socket> socket) {
  Ptr<Packet> packet;
  Address senderAddress;
  while ((packet = socket->RecvFrom (senderAddress)))
    {
      bytesTotal += packet->GetSize ();
      packetsReceived += 1;
      NS_LOG_UNCOND (PrintReceivedPacket (socket, packet, senderAddress));
    }
}

void RoutingExperiment::Run (int nSinks, double txp, int nWifis) {
  Packet::EnablePrinting();

  // Parameter: number of nodes per cluster
  //int nWifis = 12;
  // Parameter: number of cluster 
  int nCluster = 2;

  // number repetitions
  double TotalTime = 50.0;
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

  // setting up wifi phy and channel using helpers
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel(wifiChannel.Create ());

  // Add a mac and disable rate control
  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));

  wifiPhy.Set ("TxPowerStart",DoubleValue (txp));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txp));

  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer nLayer2 = wifi.Install(wifiPhy, wifiMac, layer2);
  NetDeviceContainer adhocDevices = wifi.Install(wifiPhy, wifiMac, adhocNodes);
  NetDeviceContainer adhocDevices2 = wifi.Install (wifiPhy, wifiMac, adhocNodes2);

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

  OlsrHelper olsr;
  Ipv4ListRoutingHelper list;
  InternetStackHelper internet;

  list.Add (olsr, 100);
  m_protocolName = "OLSR";

  internet.SetRoutingHelper(list);
  internet.Install(layer1);
  //internet.Install(adhocNodes);
  //internet.Install(adhocNodes2);

  NS_LOG_INFO ("assigning ip address");

  // SETTING NETWORK

  Ipv4AddressHelper addressAdhoc;

  addressAdhoc.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer adhocInterfaces = addressAdhoc.Assign(adhocDevices);

  addressAdhoc.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer adhocInterfaces2 = addressAdhoc.Assign(adhocDevices2);

  addressAdhoc.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer layer2Interfaces = addressAdhoc.Assign(nLayer2);

  OnOffHelper onoff1 ("ns3::UdpSocketFactory", Address());

  onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));

  // Sending Pack

  Ptr<Socket> sink = SetupPacketReceive(adhocInterfaces.GetAddress(0), adhocNodes.Get(0));
  AddressValue remoteAddress(InetSocketAddress(adhocInterfaces.GetAddress(0), port));
  onoff1.SetAttribute ("Remote", remoteAddress);


  Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
  ApplicationContainer temp = onoff1.Install(adhocNodes.Get(1));
  temp.Start (Seconds (var -> GetValue (100.0,101.0)));
  temp.Stop (Seconds (TotalTime));
    
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

  NS_LOG_INFO ("Run Simulation.");

  Simulator::Stop(Seconds (TotalTime));
  Simulator::Run();

  Simulator::Destroy();

}

int main() {
  int n1 = 8;
  RoutingExperiment experiment;

  //int nSinks = 10; //nSinks should be less than #Nodes
  int nSinks = 8;
  double txp = 7.5;

  experiment.Run (nSinks, txp, n1);

}