#include <fstream>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;
using namespace dsr;

int main(int argc, char *argv[])
{
  int nodePause = 0;
  int nodeSpeed = 20; //in m/s

  uint32_t nWifi = 6;
  uint32_t nCluster = 6;

  NodeContainer firstLayer;
  NodeContainer secondLayer;
  firstLayer.Create(nCluster * nWifi);

  YansWifiChannelHelper channel;

  channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  YansWifiPhyHelper phy;

  phy.SetChannel(channel.Create());
  WifiHelper wifi;
  WifiMacHelper mac;

  InternetStackHelper stack;
  OlsrHelper olsr;
  stack.SetRoutingHelper(olsr);
  Ipv4AddressHelper ipv4;

  MobilityHelper mobility;
  int64_t streamIndex = 0; // used to get consistent mobility across scenarios

  ObjectFactory pos;
  pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
  // Parameter: Geographical space -> 500m x 500m
  pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
  pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));

  Ptr<PositionAllocator> taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
  streamIndex += taPositionAlloc->AssignStreams (streamIndex);

  std::stringstream ssSpeed;
  ssSpeed << "ns3::UniformRandomVariable[Min=0.0|Max=" << nodeSpeed << "]";
  std::stringstream ssPause;
  ssPause << "ns3::ConstantRandomVariable[Constant=" << nodePause << "]";
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue (ssSpeed.str ()),
                                  "Pause", StringValue (ssPause.str ()),
                                  "PositionAllocator", PointerValue (taPositionAlloc));
  mobility.SetPositionAllocator (taPositionAlloc);
  mobility.Install (firstLayer);
  streamIndex += mobility.AssignStreams (firstLayer, streamIndex);
  NS_UNUSED (streamIndex); // From this point, streamIndex is unused

  for (uint32_t i = 0; i < nCluster; i++)
  {
    NodeContainer cluster;
    // Ipv4Address netAddress = Ipv4Address(netAddress.Get());
    for (uint32_t j = 0; j < nWifi; j++)
    {
      cluster.Add(firstLayer.Get(j + i * nWifi));
    }
    secondLayer.Add(cluster.Get(0));
    NetDeviceContainer clusterStaDevices;
    clusterStaDevices = wifi.Install(phy, mac, cluster);
    mobility.Install(cluster);
    stack.Install(cluster);
    /*
          ipv4.SetBase(Ipv4Address(netAddress), "255.255.255.0");
          Ipv4InterfaceContainer wiftInterfaces = ipv4.Assign(clusterStaDevices);
          */
  }
}
