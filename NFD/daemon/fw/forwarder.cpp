/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2015,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "forwarder.hpp"
#include "core/logger.hpp"
#include "core/random.hpp"
#include "strategy.hpp"
#include "face/null-face.hpp"
#include "ns3/ndnSIM/model/ndn-common.hpp"
#include "ns3/ndnSIM/ndn-cxx/signature.hpp"
#include "utils/ndn-ns3-packet-tag.hpp"
#include <iostream>

#include <boost/random/uniform_int_distribution.hpp>

namespace nfd {

NFD_LOG_INIT("Forwarder");

using fw::Strategy;

const Name Forwarder::LOCALHOST_NAME("ndn:/localhost");

//OON
NAME_MAP
Forwarder::init_name_map(){
  NAME_MAP  temp;// test default
  temp["_50"] = 1;
  temp["_100"] = 2;
  temp["_150"] = 3;
  temp["_200"] = 4;
  temp["_250"] = 5;
  temp["_300"] = 6;
  temp["_400"] = 7;
  temp["_500"] = 8;
  temp["_600"] = 9;
  temp["_700"] = 10;
  temp["_900"] = 11;
  temp["_1200"] = 12;
  temp["_1500"] = 13;
  temp["_2000"] = 14;
  temp["_2500"] = 15;
  temp["_3000"] = 16;
  temp["_4000"] = 17;
  temp["_5000"] = 18;
  temp["_6000"] =19;
  temp["_8000"] = 20;
  return temp;
}


RENAME_MAP
Forwarder::init_rename_map(){
  RENAME_MAP  temp;
  temp[1]="_50";
  temp[2]="_100";
  temp[3]="_150";
  temp[4]="_200";
  temp[5]="_250";
  temp[6]="_300";
  temp[7]="_400";
  temp[8]="_500";
  temp[9]="_600";
  temp[10]="_700";
  temp[11]="_900";
  temp[12]="_1200";
  temp[13]="_1500";
  temp[14]="_2000";
  temp[15]="_2500";
  temp[16]="_3000";
  temp[17]="_4000";
  temp[18]="_5000";
  temp[19]="_6000";
  temp[20] = "_8000";
  return temp;
}

NAME_MAP Forwarder::name_map(Forwarder::init_name_map());
RENAME_MAP Forwarder::rename_map(Forwarder::init_rename_map());

Forwarder::Forwarder()
  : m_faceTable(*this)
  , m_fib(m_nameTree)
  , m_pit(m_nameTree)
  , m_measurements(m_nameTree)
  , m_strategyChoice(m_nameTree, fw::makeDefaultStrategy(*this))
  , m_csFace(make_shared<NullFace>(FaceUri("contentstore://")))
  ,m_opFace(make_shared<NullFace>(FaceUri("objectprocessor://")))
{
  fw::installStrategies(*this);
  getFaceTable().addReserved(m_csFace, FACEID_CONTENT_STORE);
  getFaceTable().addReserved(m_opFace,FACEID_OBJECT_PROCESSOR);
}

Forwarder::~Forwarder()
{

}

void
Forwarder::onIncomingInterest(Face& inFace, const Interest& interest)
{
  // receive Interest
  NFD_LOG_DEBUG("onIncomingInterest face=" << inFace.getId() <<
                " interest=" << interest.getName());
  const_cast<Interest&>(interest).setIncomingFaceId(inFace.getId());
  ++m_counters.getNInInterests();

  // /localhost scope control
  bool isViolatingLocalhost = !inFace.isLocal() &&
                              LOCALHOST_NAME.isPrefixOf(interest.getName());
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onIncomingInterest face=" << inFace.getId() <<
                  " interest=" << interest.getName() << " violates /localhost");
    // (drop)
    return;
  }

  // PIT insert
  shared_ptr<pit::Entry> pitEntry = m_pit.insert(interest).first;

  // detect duplicate Nonce
  int dnw = pitEntry->findNonce(interest.getNonce(), inFace);
  bool hasDuplicateNonce = (dnw != pit::DUPLICATE_NONCE_NONE) ||
                           m_deadNonceList.has(interest.getName(), interest.getNonce());
  if (hasDuplicateNonce) {
    // goto Interest loop pipeline
    this->onInterestLoop(inFace, interest, pitEntry);
    return;
  }

  // cancel unsatisfy & straggler timer
  this->cancelUnsatisfyAndStragglerTimer(pitEntry);

  // is pending?
  const pit::InRecordCollection& inRecords = pitEntry->getInRecords();
  bool isPending = inRecords.begin() != inRecords.end();
  if (!isPending) {
    if (m_csFromNdnSim == nullptr) {
      /*Cs::find(const Interest& interest,
         const HitCallback& hitCallback,
         const MissCallback& missCallback) const*/
      /*
        Forwarder::onContentStoreHit(const Face& inFace,
                             shared_ptr<pit::Entry> pitEntry,
                             const Interest& interest,
                             const Data& data)

        void
        Forwarder::onContentStoreMiss(const Face& inFace,
                              shared_ptr<pit::Entry> pitEntry,
                              const Interest& interest)
        onProcessingData(const Face& inFace, const Interest& interest, const Data& data);
      */

      m_cs.find(interest,
                bind(&Forwarder::onContentStoreHit, this, ref(inFace), pitEntry, _1, _2),
                bind(&Forwarder::onObjectProcessorHit, this, ref(inFace), pitEntry, _1));
      //OON
    }
    else {
      shared_ptr<Data> match = m_csFromNdnSim->Lookup(interest.shared_from_this());
      if (match != nullptr) {
        this->onContentStoreHit(inFace, pitEntry, interest, *match);
      }
      else {
        this->onObjectProcessorHit(inFace, pitEntry, interest);
      }
    }
  }
  else {
    this->onObjectProcessorMiss(inFace, pitEntry, interest);
  }
}

void
Forwarder::onObjectProcessorMiss(const Face& inFace,
                              shared_ptr<pit::Entry> pitEntry,
                              const Interest& interest)
{
  NFD_LOG_DEBUG("onContentStoreMiss interest=" << interest.getName());

  shared_ptr<Face> face = const_pointer_cast<Face>(inFace.shared_from_this());
  // insert InRecord
  pitEntry->insertOrUpdateInRecord(face, interest);

  // set PIT unsatisfy timer
  this->setUnsatisfyTimer(pitEntry);

  // FIB lookup
  shared_ptr<fib::Entry> fibEntry = m_fib.findLongestPrefixMatch(*pitEntry);

  // dispatch to strategy
  this->dispatchToStrategy(pitEntry, bind(&Strategy::afterReceiveInterest, _1,
                                          cref(inFace), cref(interest), fibEntry, pitEntry));
}

void
Forwarder::onContentStoreMiss(const Face& inFace,
                              shared_ptr<pit::Entry> pitEntry, const Interest& child_interest,
                              const Interest& parent_interest){

  // NFD_LOG_DEBUG("onContentStoreMiss interest=" << parent_interest.getName());

  // shared_ptr<Face> face = const_pointer_cast<Face>(inFace.shared_from_this());
  // // insert InRecord
  // pitEntry->insertOrUpdateInRecord(face, parent_interest);

  // // set PIT unsatisfy timer
  // this->setUnsatisfyTimer(pitEntry);

  // // FIB lookup
  // shared_ptr<fib::Entry> fibEntry = m_fib.findLongestPrefixMatch(*pitEntry);

  // // dispatch to strategy
  // this->dispatchToStrategy(pitEntry, bind(&Strategy::afterReceiveInterest, _1,
  //                                         cref(inFace), cref(parent_interest), fibEntry, pitEntry));
}

void
Forwarder::onObjectProcessorHit(const Face& inFace,
                              shared_ptr<pit::Entry> pitEntry,
                              const Interest& child_interest)
{
    Name childName(child_interest.getName());
    childName = childName.getPrefix(-1);
    std::string childname = childName.toUri();  // get the uri from interest
    std::string parent_fname;
    std::string movie = "bunny_2s";
    size_t pos_1 = childname.find(movie);
    bool tag = false;
    size_t pos_2 = childname.find("kbit",pos_1+1);
    if (pos_1 != std::string::npos && pos_2 != std::string::npos){
      std::string prefix = childname.substr(0, pos_1+8);
      std::string suffix = childname.substr(pos_2);;
      std::string quality = childname.substr(pos_1+8,pos_2-pos_1-8);
      tag = false;
      shared_ptr<Data> match = nullptr;
      uint index = name_map[childname.substr(pos_1+8,pos_2-pos_1-8)];
      std::string parentname;
      while (!tag && match == nullptr && index <20){
        index ++;
         parentname = rename_map[index];
         parentname = prefix+parentname+suffix;
        //std::cout<<parentname<<std::endl;
        //shared_ptr<Name> nameWithSequence = make_shared<Name>(child_interest.getName());
        //nameWithSequence->appendSequenceNumber(seq);
        shared_ptr<Interest> parent_interest = make_shared<Interest>();
        parent_interest->setNonce(child_interest.getNonce());
        parent_interest->setName(parentname);
        parent_interest->setInterestLifetime(child_interest.getInterestLifetime());
        //NS_LOG_INFO("Creating INTEREST for " << parent_interest.getName());
        shared_ptr<pit::Entry> new_pitEntry = m_pit.insert(*parent_interest).first; //waiting for the parent data in the near future
        if (m_opFromNdnSim == nullptr ) {
          m_op.find(*parent_interest,
                    bind(&Forwarder::onProcessingData, this, ref(inFace), _1, &tag , child_interest,_2),
                    bind(&Forwarder::onContentStoreMiss, this, ref(inFace), new_pitEntry, child_interest, _1));
        } 
        else {
          match = m_opFromNdnSim->Lookup(parent_interest);
          if (match != nullptr) {
            this->onProcessingData(inFace, *parent_interest, &tag, child_interest,*match);
          }
          else{
            this->onContentStoreMiss(inFace,new_pitEntry,child_interest,*parent_interest);
          }
        }
      }
    }
    if (!tag){
      this->onObjectProcessorMiss(inFace, pitEntry, child_interest);
    }
    return;
}


void
Forwarder::onContentStoreHit(const Face& inFace,
                             shared_ptr<pit::Entry> pitEntry,
                             const Interest& interest,
                             const Data& data)
{
  NFD_LOG_DEBUG("onContentStoreHit interest=" << interest.getName());

  beforeSatisfyInterest(*pitEntry, *m_csFace, data);
  this->dispatchToStrategy(pitEntry, bind(&Strategy::beforeSatisfyInterest, _1,
                                          pitEntry, cref(*m_csFace), cref(data)));

  const_pointer_cast<Data>(data.shared_from_this())->setIncomingFaceId(FACEID_CONTENT_STORE);

  this->setStragglerTimer(pitEntry, true, data.getFreshnessPeriod());

  // goto outgoing Data pipeline
  this->onOutgoingData(data, *const_pointer_cast<Face>(inFace.shared_from_this()));
}

//OON
void
Forwarder:: onProcessingData(const Face& inFace, const Interest& parent_interest, bool* tag, const Interest& child_interest, const Data& parent_data)
{   
    NFD_LOG_DEBUG("onProcessingData parent interest=" << parent_interest.getName());
    *tag = true;
    // Get seq_nr from Interest Name
    //Name childName = child_interest.getName();
    //std::cout<<"processing..."<<parent_interest.getName()<<std::endl;
    //std::cout<<"data name is"<<parent_data.getName()<<std::endl;
    //std::cout<<"parent data size is "<<parent_data.getContent().size()<<std::endl;
   // uint32_t seqNo = childName.at(-1).toSequenceNumber();

    auto child_data = make_shared<Data>();
    child_data->setName(child_interest.getName());
    auto buffer = make_shared< ::ndn::Buffer>(parent_data.getContent().size()-4);
    child_data->setContent(buffer);

    ndn::Signature signature;
    ndn::SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

    signature.setInfo(signatureInfo);
    signature.setValue(::ndn::nonNegativeIntegerBlock(::ndn::tlv::SignatureValue, 0)); //default value is 0, others are application specific

    child_data->setSignature(signature);


    // to create real wire encoding
    Block tmp = child_data->wireEncode();
    //std::cout<<"child size: "<<child_data->getContent().size()<<"     "<<"parent size: "<<parent_data.getContent().size()<<std::endl;
    // volatile size_t i = 1;
    // size_t size = data.getContent().value_size();
    // for (i = 1; i < size; i++); //to do data processing
    // onOutgoingData(data,outFace);
    //std::cout<<"processing..."<<data.getName()<<std::endl;
    this->onOutgoingData(*child_data, *const_pointer_cast<Face>(inFace.shared_from_this()));
    const_pointer_cast<Data>(child_data->shared_from_this())->setIncomingFaceId(FACEID_OBJECT_PROCESSOR);
    // if (inFace.getId() == INVALID_FACEID) {
    //    NFD_LOG_WARN("onOutgoingData face=invalid data=" << data.getName());
    //    return;
    // //   //drop
    //  }
    //  NFD_LOG_DEBUG("onOutgoingData face=" << inFace.getId() << " data=" << data.getName());
    //  const_pointer_cast<Face>(inFace.shared_from_this())->sendData(data);
    //  ++m_counters.getNOutDatas();
    shared_ptr<Data> dataCopyWithoutPacket = make_shared<Data>(*child_data);
    dataCopyWithoutPacket->removeTag<ns3::ndn::Ns3PacketTag>();
    if (m_csFromNdnSim == nullptr){
        m_cs.insert(*dataCopyWithoutPacket);
      }
      else{
        m_csFromNdnSim->Add(dataCopyWithoutPacket);
      }

    return;
  }


void
Forwarder::onInterestLoop(Face& inFace, const Interest& interest,
                          shared_ptr<pit::Entry> pitEntry)
{
  NFD_LOG_DEBUG("onInterestLoop face=" << inFace.getId() <<
                " interest=" << interest.getName());
    return;
}


/** \brief compare two InRecords for picking outgoing Interest
 *  \return true if b is preferred over a
 *
 *  This function should be passed to std::max_element over InRecordCollection.
 *  The outgoing Interest picked is the last incoming Interest
 *  that does not come from outFace.
 *  If all InRecords come from outFace, it's fine to pick that. This happens when
 *  there's only one InRecord that comes from outFace. The legit use is for
 *  vehicular network; otherwise, strategy shouldn't send to the sole inFace.
 */
static inline bool
compare_pickInterest(const pit::InRecord& a, const pit::InRecord& b, const Face* outFace)
{
  bool isOutFaceA = a.getFace().get() == outFace;
  bool isOutFaceB = b.getFace().get() == outFace;

  if (!isOutFaceA && isOutFaceB) {
    return false;
  }
  if (isOutFaceA && !isOutFaceB) {
    return true;
  }

  return a.getLastRenewed() > b.getLastRenewed();
}

void
Forwarder::onOutgoingInterest(shared_ptr<pit::Entry> pitEntry, Face& outFace,
                              bool wantNewNonce)
{
  if (outFace.getId() == INVALID_FACEID) {
    NFD_LOG_WARN("onOutgoingInterest face=invalid interest=" << pitEntry->getName());
    return;
  }
  NFD_LOG_DEBUG("onOutgoingInterest face=" << outFace.getId() <<
                " interest=" << pitEntry->getName());

  // scope control
  if (pitEntry->violatesScope(outFace)) {
    NFD_LOG_DEBUG("onOutgoingInterest face=" << outFace.getId() <<
                  " interest=" << pitEntry->getName() << " violates scope");
    return;
  }

  // pick Interest
  const pit::InRecordCollection& inRecords = pitEntry->getInRecords();
  pit::InRecordCollection::const_iterator pickedInRecord = std::max_element(
    inRecords.begin(), inRecords.end(), bind(&compare_pickInterest, _1, _2, &outFace));
  BOOST_ASSERT(pickedInRecord != inRecords.end());
  shared_ptr<Interest> interest = const_pointer_cast<Interest>(
    pickedInRecord->getInterest().shared_from_this());

  if (wantNewNonce) {
    interest = make_shared<Interest>(*interest);
    static boost::random::uniform_int_distribution<uint32_t> dist;
    interest->setNonce(dist(getGlobalRng()));
  }

  // insert OutRecord
  pitEntry->insertOrUpdateOutRecord(outFace.shared_from_this(), *interest);

  // send Interest
  outFace.sendInterest(*interest);
  ++m_counters.getNOutInterests();
}

void
Forwarder::onInterestReject(shared_ptr<pit::Entry> pitEntry)
{
  if (pitEntry->hasUnexpiredOutRecords()) {
    NFD_LOG_ERROR("onInterestReject interest=" << pitEntry->getName() <<
                  " cannot reject forwarded Interest");
    return;
  }
  NFD_LOG_DEBUG("onInterestReject interest=" << pitEntry->getName());

  // cancel unsatisfy & straggler timer
  this->cancelUnsatisfyAndStragglerTimer(pitEntry);

  // set PIT straggler timer
  this->setStragglerTimer(pitEntry, false);
}

void
Forwarder::onInterestUnsatisfied(shared_ptr<pit::Entry> pitEntry)
{
  NFD_LOG_DEBUG("onInterestUnsatisfied interest=" << pitEntry->getName());

  // invoke PIT unsatisfied callback
  beforeExpirePendingInterest(*pitEntry);
  this->dispatchToStrategy(pitEntry, bind(&Strategy::beforeExpirePendingInterest, _1,
                                          pitEntry));

  // goto Interest Finalize pipeline
  this->onInterestFinalize(pitEntry, false);
}

void
Forwarder::onInterestFinalize(shared_ptr<pit::Entry> pitEntry, bool isSatisfied,
                              const time::milliseconds& dataFreshnessPeriod)
{
  NFD_LOG_DEBUG("onInterestFinalize interest=" << pitEntry->getName() <<
                (isSatisfied ? " satisfied" : " unsatisfied"));

  // Dead Nonce List insert if necessary
  this->insertDeadNonceList(*pitEntry, isSatisfied, dataFreshnessPeriod, 0);

  // PIT delete
  this->cancelUnsatisfyAndStragglerTimer(pitEntry);
  m_pit.erase(pitEntry);
}

void
Forwarder::onIncomingData(Face& inFace, const Data& data)
{
  // receive Data
  NFD_LOG_DEBUG("onIncomingData face=" << inFace.getId() << " data=" << data.getName());
  const_cast<Data&>(data).setIncomingFaceId(inFace.getId());
  ++m_counters.getNInDatas();

  // /localhost scope control
  bool isViolatingLocalhost = !inFace.isLocal() &&
                              LOCALHOST_NAME.isPrefixOf(data.getName());
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onIncomingData face=" << inFace.getId() <<
                  " data=" << data.getName() << " violates /localhost");
    // (drop)
    return;
  }

  // PIT match
  pit::DataMatchResult pitMatches = m_pit.findAllDataMatches(data);
  if (pitMatches.begin() == pitMatches.end()) {
    // goto Data unsolicited pipeline
    this->onDataUnsolicited(inFace, data);
    return;
  }

  // Remove Ptr<Packet> from the Data before inserting into cache, serving two purposes
  // - reduce amount of memory used by cached entries
  // - remove all tags that (e.g., hop count tag) that could have been associated with Ptr<Packet>
  //
  // Copying of Data is relatively cheap operation, as it copies (mostly) a collection of Blocks
  // pointing to the same underlying memory buffer.
  shared_ptr<Data> dataCopyWithoutPacket = make_shared<Data>(data);
  dataCopyWithoutPacket->removeTag<ns3::ndn::Ns3PacketTag>();

  // CS insert
  if (m_csFromNdnSim == nullptr){
      m_cs.insert(*dataCopyWithoutPacket);
    }
    else{
      m_csFromNdnSim->Add(dataCopyWithoutPacket);
    }
    // OP insert, specifically for DASH
    if(m_opFromNdnSim == nullptr){
        m_op.insert(*dataCopyWithoutPacket);
    }else{
        m_opFromNdnSim->Add(dataCopyWithoutPacket);
    }

  std::set<shared_ptr<Face> > pendingDownstreams;
  // foreach PitEntry
  for (const shared_ptr<pit::Entry>& pitEntry : pitMatches) {
    NFD_LOG_DEBUG("onIncomingData matching=" << pitEntry->getName());

    // cancel unsatisfy & straggler timer
    this->cancelUnsatisfyAndStragglerTimer(pitEntry);

    // remember pending downstreams
    const pit::InRecordCollection& inRecords = pitEntry->getInRecords();
    for (pit::InRecordCollection::const_iterator it = inRecords.begin();
                                                 it != inRecords.end(); ++it) {
      if (it->getExpiry() > time::steady_clock::now()) {
        pendingDownstreams.insert(it->getFace());
      }
    }

    // invoke PIT satisfy callback
    beforeSatisfyInterest(*pitEntry, inFace, data);
    this->dispatchToStrategy(pitEntry, bind(&Strategy::beforeSatisfyInterest, _1,
                                            pitEntry, cref(inFace), cref(data)));

    // Dead Nonce List insert if necessary (for OutRecord of inFace)
    this->insertDeadNonceList(*pitEntry, true, data.getFreshnessPeriod(), &inFace);

    // mark PIT satisfied
    pitEntry->deleteInRecords();
    pitEntry->deleteOutRecord(inFace);

    // set PIT straggler timer
    this->setStragglerTimer(pitEntry, true, data.getFreshnessPeriod());
  }

  // foreach pending downstream
  for (std::set<shared_ptr<Face> >::iterator it = pendingDownstreams.begin();
      it != pendingDownstreams.end(); ++it) {
    shared_ptr<Face> pendingDownstream = *it;
    if (pendingDownstream.get() == &inFace) {
      continue;
    }
    // goto outgoing Data pipeline
    this->onOutgoingData(data, *pendingDownstream);
  }
}

void
Forwarder::onDataUnsolicited(Face& inFace, const Data& data)
{
  // accept to cache?
  bool acceptToCache = inFace.isLocal();
  if (acceptToCache) {
    // CS insert
    if (m_csFromNdnSim == nullptr){
      m_cs.insert(data, true);
    }
    else{
      m_csFromNdnSim->Add(data.shared_from_this());
    }
    // OP insert, specificate for DASH
    if (m_opFromNdnSim == nullptr){
      m_op.insert(data,true);
    }  
    else{
      m_opFromNdnSim->Add(data.shared_from_this());
    }
  }

  NFD_LOG_DEBUG("onDataUnsolicited face=" << inFace.getId() <<
                " data=" << data.getName() <<
                (acceptToCache ? " cached" : " not cached"));
}

void
Forwarder::onOutgoingData(const Data& data, Face& outFace)
{
  if (outFace.getId() == INVALID_FACEID) {
    NFD_LOG_WARN("onOutgoingData face=invalid data=" << data.getName());
    return;
  }
  NFD_LOG_DEBUG("onOutgoingData face=" << outFace.getId() << " data=" << data.getName());

  // /localhost scope control
  bool isViolatingLocalhost = !outFace.isLocal() &&
                              LOCALHOST_NAME.isPrefixOf(data.getName());
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onOutgoingData face=" << outFace.getId() <<
                  " data=" << data.getName() << " violates /localhost");
    // (drop)
    return;
  }
  // TODO traffic manager

  // send Data
  outFace.sendData(data);
  ++m_counters.getNOutDatas();
}


static inline bool
compare_InRecord_expiry(const pit::InRecord& a, const pit::InRecord& b)
{
  return a.getExpiry() < b.getExpiry();
}

void
Forwarder::setUnsatisfyTimer(shared_ptr<pit::Entry> pitEntry)
{
  const pit::InRecordCollection& inRecords = pitEntry->getInRecords();
  pit::InRecordCollection::const_iterator lastExpiring =
    std::max_element(inRecords.begin(), inRecords.end(),
    &compare_InRecord_expiry);

  time::steady_clock::TimePoint lastExpiry = lastExpiring->getExpiry();
  time::nanoseconds lastExpiryFromNow = lastExpiry  - time::steady_clock::now();
  if (lastExpiryFromNow <= time::seconds(0)) {
    // TODO all InRecords are already expired; will this happen?
  }

  scheduler::cancel(pitEntry->m_unsatisfyTimer);
  pitEntry->m_unsatisfyTimer = scheduler::schedule(lastExpiryFromNow,
    bind(&Forwarder::onInterestUnsatisfied, this, pitEntry));
}

void
Forwarder::setStragglerTimer(shared_ptr<pit::Entry> pitEntry, bool isSatisfied,
                             const time::milliseconds& dataFreshnessPeriod)
{
  time::nanoseconds stragglerTime = time::milliseconds(100);

  scheduler::cancel(pitEntry->m_stragglerTimer);
  pitEntry->m_stragglerTimer = scheduler::schedule(stragglerTime,
    bind(&Forwarder::onInterestFinalize, this, pitEntry, isSatisfied, dataFreshnessPeriod));
}

void
Forwarder::cancelUnsatisfyAndStragglerTimer(shared_ptr<pit::Entry> pitEntry)
{
  scheduler::cancel(pitEntry->m_unsatisfyTimer);
  scheduler::cancel(pitEntry->m_stragglerTimer);
}

static inline void
insertNonceToDnl(DeadNonceList& dnl, const pit::Entry& pitEntry,
                 const pit::OutRecord& outRecord)
{
  dnl.add(pitEntry.getName(), outRecord.getLastNonce());
}

void
Forwarder::insertDeadNonceList(pit::Entry& pitEntry, bool isSatisfied,
                               const time::milliseconds& dataFreshnessPeriod,
                               Face* upstream)
{
  // need Dead Nonce List insert?
  bool needDnl = false;
  if (isSatisfied) {
    bool hasFreshnessPeriod = dataFreshnessPeriod >= time::milliseconds::zero();
    // Data never becomes stale if it doesn't have FreshnessPeriod field
    needDnl = static_cast<bool>(pitEntry.getInterest().getMustBeFresh()) &&
              (hasFreshnessPeriod && dataFreshnessPeriod < m_deadNonceList.getLifetime());
  }
  else {
    needDnl = true;
  }

  if (!needDnl) {
    return;
  }

  // Dead Nonce List insert
  if (upstream == 0) {
    // insert all outgoing Nonces
    const pit::OutRecordCollection& outRecords = pitEntry.getOutRecords();
    std::for_each(outRecords.begin(), outRecords.end(),
                  bind(&insertNonceToDnl, ref(m_deadNonceList), cref(pitEntry), _1));
  }
  else {
    // insert outgoing Nonce of a specific face
    pit::OutRecordCollection::const_iterator outRecord = pitEntry.getOutRecord(*upstream);
    if (outRecord != pitEntry.getOutRecords().end()) {
      m_deadNonceList.add(pitEntry.getName(), outRecord->getLastNonce());
    }
  }
}

} // namespace nfd
