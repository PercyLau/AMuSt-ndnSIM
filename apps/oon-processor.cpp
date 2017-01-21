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

#include "oon-processor.hpp"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include "ns3/data-rate.h"
#include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"

#include "model/ndn-app-face.hpp"
#include "model/ndn-ns3.hpp"
#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"

#include <memory>
#include <sys/types.h>
#include <sys/stat.h>

#include <math.h>

#include <iostream>


NS_LOG_COMPONENT_DEFINE("oon.processor");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(Processor);

TypeId
Processor::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::Processor")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddConstructor<Processor>()
      .AddAttribute("Prefix", "Prefix, for which object processor serves the data", StringValue("/ObjectProcessor/"),
                    MakeStringAccessor(&Processor::m_prefix), MakeStringChecker())
      .AddAttribute("ContentDirectory", "The directory of which processor serves the files", StringValue("/"),
                    MakeStringAccessor(&Processor::m_contentDir), MakeStringChecker())
      .AddAttribute("ObjectProcessor", "The directory of which processor serves the files", StringValue("/"),
                    MakeStringAccessor(&Processor::m_processorInterface), MakeStringChecker())
      .AddAttribute("ManifestPostfix", "The manifest string added after a file", StringValue("/manifest"),
                    MakeStringAccessor(&Processor::m_postfixManifest), MakeStringChecker())
      .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(0)), MakeTimeAccessor(&Processor::m_freshness),
                    MakeTimeChecker())
      .AddAttribute(
         "Signature",
         "Fake signature, 0 valid signature (default), other values application-specific",
         UintegerValue(0), MakeUintegerAccessor(&Processor::m_signature),
         MakeUintegerChecker<uint32_t>())
      .AddAttribute("KeyLocator",
                    "Name to be used for key locator.  If root, then key locator is not used",
                    NameValue(), MakeNameAccessor(&Processor::m_keyLocator), MakeNameChecker());
  return tid;
}

//OON
NAME_MAP
Processor::init_name_map(){
  NAME_MAP  temp;
  temp["_test"] = 0; // test default
  temp["_50kbit"] = 1;
  temp["_100kbit"] = 2;
  temp["_150kbit"] = 3;
  temp["_200kbit"] = 4;
  temp["_250kbit"] = 5;
  temp["_300kbit"] = 6;
  temp["_400kbit"] = 7;
  temp["_500kbit"] = 8;
  temp["_600kbit"] = 9;
  temp["_700kbit"] = 10;
  temp["_900kbit"] = 11;
  temp["_1200kbit"] = 12;
  temp["_1500kbit"] = 13;
  temp["_2000kbit"] = 14;
  temp["_2500kbit"] = 15;
  temp["_3000kbit"] = 16;
  temp["_4000kbit"] = 17;
  temp["_5000kbit"] = 18;
  temp["_6000kbit"] =19;
  temp["_8000kbit"] = 20;
  return temp;
}


RENAME_MAP
Processor::init_rename_map(){
  RENAME_MAP  temp;
  temp[0] = "_success";
  temp[1]="_50kbit";
  temp[2]="_100kbit";
  temp[3]="_150kbit";
  temp[4]="_200kbit";
  temp[5]="_250kbit";
  temp[6]="_300kbit";
  temp[7]="_400kbit";
  temp[8]="_500kbit";
  temp[9]="_600kbit";
  temp[10]="_700kbit";
  temp[11]="_900kbit";
  temp[12]="_1200kbit";
  temp[13]="_1500kbit";
  temp[14]="_2000kbit";
  temp[15]="_2500kbit";
  temp[16]="_3000kbit";
  temp[17]="_4000kbit";
  temp[18]="_5000kbit";
  temp[19]="_6000kbit";
  temp[20] = "_8000kbit";
  return temp;
}

NAME_MAP Processor::name_map(Processor::init_name_map());
RENAME_MAP Processor::rename_map(Processor::init_rename_map());


Processor::Processor()
{
  NS_LOG_FUNCTION_NOARGS();
}

// inherited from Application base class.
void
Processor::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StartApplication();

  FibHelper::AddRoute(GetNode(), m_prefix, m_face, 0);
  m_MTU = GetFaceMTU(0);
}

void
Processor::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StopApplication();
}



uint16_t
Processor::GetFaceMTU(uint32_t faceId)
{
  //todo: a compatible mtu get function. here we cannot use point-to-point netdevice anymore
  Ptr<ns3::NetDevice> nd1 = GetNode()->GetDevice(faceId)->GetObject<ns3::NetDevice>();
  return nd1->GetMtu();
}


//OON part, this can used to recoginize interest and rename it
void
Processor::OnInterest(shared_ptr<const Interest> interest)
{
  App::OnInterest(interest); // tracing inside
  NS_LOG_FUNCTION(this << interest);

  if (!m_active)
    return;

  Name dataName(interest->getName());
  //std::cout<<dataName.toUri()<<std::endl;
  // get last postfix
  ndn::Name  lastPostfix = dataName.getSubName(dataName.size() -1 );
  NS_LOG_INFO("> LastPostfix = " << lastPostfix);

  bool isManifest = false;
  uint32_t seqNo = -1;

  if (lastPostfix == m_postfixManifest)
  {
    isManifest = true;
  }
  else
  {
    seqNo = dataName.at(-1).toSequenceNumber();
    seqNo = seqNo - 1; // Christian: the client thinks seqNo = 1 is the first one, for the server it's better to start at 0
  }

  //std::cout<<seqNo<<std::endl;
  // remove the last postfix
  dataName = dataName.getPrefix(-1);

  // extract filename and get path to file
  std::string fname = dataName.toUri();  // get the uri from interest
  

//20161215 OON
  ///home/percy/multimediaData/AVC/BBB/bunny_2s_250kbit/bunny_2s5.m4s
  std::string new_fname;
  std::string movie = "bunny_2s";
  int pos_1 = fname.find(movie);
  int pos_2 = fname.find(movie,pos_1+1);
  if (pos_1 != std::string::npos){
  std::string prefix = fname.substr(0, pos_1+8);
  std::string suffix;
  std::string down_level;
  if (pos_2 == std::string::npos){
    suffix = fname.substr(pos_2-1);
  //std::string quality = fname.substr(pos_1+8,pos_2-pos_1-9);
    if (name_map[fname.substr(pos_1+8,pos_2-pos_1-9)]>1){
      down_level = rename_map[name_map[fname.substr(pos_1+8,pos_2-pos_1-9)]-1];
    }
    else{
      down_level = rename_map[name_map[fname.substr(pos_1+8,pos_2-pos_1-9)]];
    }
  }
  new_fname = prefix+down_level+suffix;
}else
{
  new_fname = fname;
}
  //Name new_name(new_fname);

  // measure how much overhead this actually this
  int diff = EstimateOverhead(fname);

  //OON
  int oon_diff = EstimateOverhead(new_fname);
  // set new payload size to this value (minus 4 bytes to be safe for sequence numbers, ethernet headers, etc...)
  m_maxPayloadSize = m_MTU - diff - 4;
    //OON
  //m_maxPayloadSize = m_MTU - oon_diff - 4;
  m_processorInterface = m_contentDir;
  new_fname = new_fname.substr(m_prefix.length(), new_fname.length()); // remove the prefix
  new_fname = std::string(m_processorInterface).append(new_fname); // prepend the data path


  //NS_LOG_UNCOND("NewPayload = " << m_maxPayloadSize << " (Overhead: " << diff << ") ");

  fname = fname.substr(m_prefix.length(), fname.length()); // remove the prefix
  fname = std::string(m_contentDir).append(fname); // prepend the data path

  // handle manifest or data
  if (isManifest)
  {
    NS_LOG_INFO("node(" << GetNode()->GetId() << ") responding with Manifest for file " << fname);
    std::cout<<"BBBB"<<std::endl;
    ReturnManifestData(interest, fname);
  } else
  {
    NS_LOG_INFO("node(" << GetNode()->GetId() << ") responding with Payload for file " << fname);
    NS_LOG_DEBUG("FileName: " << fname);
    NS_LOG_DEBUG("SeqNo: " << seqNo);

    // check if file exists and the sanity of the sequence number requested
    long fileSize = GetFileSize(fname);

    //OON
    long new_fileSize = GetFileSize(new_fname);

    if (fileSize == -1)
      //std::cout<<"file does not exits"<<std::endl;
      return; // file does not exist, just quit

    if (seqNo > ceil(fileSize / m_maxPayloadSize))
      return; // sequence not available

    //std::cout<<"filesize"<<m_maxPayloadSize<<"    "<<fileSize<<std::endl;
    // else:
    //OON
    if(seqNo < floor(new_fileSize/m_maxPayloadSize)){
     //ReturnPayloadData(interest, new_fname, seqNo);
    }
    std::cout<<new_fname<<std::endl;
    ReturnPayloadData(interest, fname, seqNo);
  }
}



void
Processor::ReturnManifestData(shared_ptr<const Interest> interest, std::string& fname)
{
  long fileSize = GetFileSize(fname);

  auto data = make_shared<Data>();
  data->setName(interest->getName());
  data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));

  // create a local buffer variable, which contains a long and an unsigned
  uint8_t buffer[sizeof(long) + sizeof(unsigned)];
  memcpy(buffer, &fileSize, sizeof(long));
  memcpy(buffer+sizeof(long), &m_maxPayloadSize, sizeof(unsigned));

  // create content with the file size in it
  data->setContent(reinterpret_cast<const uint8_t*>(buffer), sizeof(long) + sizeof(unsigned));

  Signature signature;
  SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

  if (m_keyLocator.size() > 0) {
    signatureInfo.setKeyLocator(m_keyLocator);
  }

  signature.setInfo(signatureInfo);
  signature.setValue(::ndn::nonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

  data->setSignature(signature);


  // to create real wire encoding
  data->wireEncode();

  m_transmittedDatas(data, this, m_face);///TracedCallback<shared_ptr<const Data>, Ptr<App>, shared_ptr<Face>> m_transmittedDFatas ///< @brief App-level trace of transmitted Data
  m_face->onReceiveData(*data);
}




// OON
void
Processor::ReturnPayloadData(shared_ptr<const Interest> interest, std::string& fname, uint32_t seqNo)
{
  auto data = make_shared<Data>();
  data->setName(interest->getName());
  data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));

  // go to pointer seqNo*m_maxPayloadSize in file
  FILE* fp = fopen(fname.c_str(), "rb");
  fseek(fp, seqNo * m_maxPayloadSize, SEEK_SET);

  auto buffer = make_shared< ::ndn::Buffer>(m_maxPayloadSize);
  //size_t actualSize = fread(buffer->get(), sizeof(uint8_t), m_maxPayloadSize, fp);
  // this is make new content
  fread(buffer->get(), sizeof(uint8_t), m_maxPayloadSize, fp);
  fclose(fp);

  /*if (actualSize < m_maxPayloadSize)
    buffer->resize(actualSize+1);*/

  data->setContent(buffer);

  Signature signature;
  SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

  if (m_keyLocator.size() > 0) {
    signatureInfo.setKeyLocator(m_keyLocator);
  }

  signature.setInfo(signatureInfo);
  signature.setValue(::ndn::nonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

  data->setSignature(signature);


  // to create real wire encoding
  Block tmp = data->wireEncode();

  m_transmittedDatas(data, this, m_face);
  m_face->onReceiveData(*data);
}

size_t
Processor::EstimateOverhead(std::string& fname)
{
  if (m_packetSizes.find(fname) != m_packetSizes.end())
  {
    return m_packetSizes[fname];
  }

  uint32_t interestLength = fname.length();
  // estimate the payload size for now
  int estimatedMaxPayloadSize = m_MTU - interestLength - 30; // the -30 is something we saw in results, it's just to estimate...

  auto data = make_shared<Data>();
  data->setName(fname + "/1"); // to simulate that there is at least one chunk
  data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));

  auto buffer = make_shared< ::ndn::Buffer>(estimatedMaxPayloadSize);
  data->setContent(buffer);

  Signature signature;
  SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

  if (m_keyLocator.size() > 0) {
    signatureInfo.setKeyLocator(m_keyLocator);
  }

  signature.setInfo(signatureInfo);
  signature.setValue(::ndn::nonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

  data->setSignature(signature);


  // to create real wire encoding
  Block tmp = data->wireEncode();

  m_packetSizes[fname] = tmp.size() - estimatedMaxPayloadSize;

  return tmp.size() - estimatedMaxPayloadSize;
}



// GetFileSize either from m_fileSizes map or from disk
long Processor::GetFileSize(std::string filename)
{
  // check if is already in m_fileSizes
  if (m_fileSizes.find(filename) != m_fileSizes.end())
  {
    return m_fileSizes[filename];
  }
  // else: query disk for file size

  struct stat stat_buf;
  int rc = stat(filename.c_str(), &stat_buf);

  if (rc == 0)
  {
    m_fileSizes[filename] = stat_buf.st_size;
    return stat_buf.st_size;
  }
  // else: file not found
  NS_LOG_UNCOND("ERROR: File not found: " << filename);
  return -1;
}





} // namespace ndn
} // namespace ns3
