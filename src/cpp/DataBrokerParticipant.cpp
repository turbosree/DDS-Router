// Copyright 2021 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file DataBrokerParticipant.cpp
 *
 */

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/rtps/transport/TCPv4TransportDescriptor.h>
#include <fastdds/rtps/transport/UDPv4TransportDescriptor.h>

#include <StdString/StdStringPubSubTypes.h>

#include <databroker/DataBrokerParticipant.hpp>

namespace eprosima {
namespace databroker {

DataBrokerParticipant::DataBrokerParticipant(
        eprosima::fastdds::dds::DomainParticipantListener* listener,
        DataBrokerParticipantConfiguration configuration)
    : listener_(listener)
    , configuration_(configuration)
    , type_(new StdStringPubSubType())
{
}

DataBrokerParticipant::DataBrokerParticipant(
        eprosima::fastdds::dds::DomainParticipantListener* listener)
    : listener_(listener)
    , type_(new StdStringPubSubType())
{
}

DataBrokerParticipant::~DataBrokerParticipant()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    logInfo(DATABROKER_PARTICIPANT, "Destroying Participant " << name());

    if (enabled_)
    {
        for (auto writer : datawriters_)
        {
            publisher_->delete_datawriter(writer.second);
        }
        datawriters_.clear();

        for (auto reader : datareaders_)
        {
            subscriber_->delete_datareader(reader.second);
        }
        datareaders_.clear();

        participant_->delete_publisher(publisher_);

        participant_->delete_subscriber(subscriber_);

        for (auto topic : topics_)
        {
            participant_->delete_topic(topic.second);
        }
        topics_.clear();

        eprosima::fastdds::dds::DomainParticipantFactory::get_instance()->delete_participant(participant_);
    }

    // Warning: Do not destroy the Listener, as it is not created in this class
}

bool DataBrokerParticipant::init()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (!enabled_)
    {
        eprosima::fastdds::dds::DomainParticipantQos pqos = participant_qos();

        // Set actual name stored
        pqos.name(fastrtps::string_255(name()));

        logInfo(DATABROKER_PARTICIPANT, "Initializing Participant '" << name() << "'");

        // Mask is needed to block data_on_readers callback
        eprosima::fastdds::dds::StatusMask mask =
                eprosima::fastdds::dds::StatusMask::data_available() <<
                eprosima::fastdds::dds::StatusMask::subscription_matched() <<
                eprosima::fastdds::dds::StatusMask::publication_matched();

        // Create Participant
        participant_ =
                eprosima::fastdds::dds::DomainParticipantFactory::get_instance()->create_participant(
            get_configuration_().domain,
            pqos,
            listener_,
            mask);

        if (!participant_)
        {
            logError(DATABROKER_PARTICIPANT, "Error initializing Participant '" << pqos.name() << "'");
            return false;
        }

        // Create publisher
        publisher_ = participant_->create_publisher(eprosima::fastdds::dds::PUBLISHER_QOS_DEFAULT);
        if (!publisher_)
        {
            logError(DATABROKER_PARTICIPANT, "Error initializing Publisher for Participant " << pqos.name());
            return false;
        }

        // Creating Subscriber
        subscriber_ = participant_->create_subscriber(eprosima::fastdds::dds::SUBSCRIBER_QOS_DEFAULT);
        if (!subscriber_)
        {
            logError(DATABROKER_PARTICIPANT, "Error initializing Subscriber for Participant " << pqos.name());
            return false;
        }

        // Registergin type
        if (!register_type_())
        {
            logError(DATABROKER_PARTICIPANT, "Error registering type in Participant " << name());
            return false;
        }
    }

    logInfo(DATABROKER_PARTICIPANT, "DataBroker Participant with name " << name() << " initialized");

    return true;
}

bool DataBrokerParticipant::enable()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (!enabled_)
    {
        if (!participant_)
        {
            logError(DATABROKER_PARTICIPANT, "Trying to enable a Participant not created");
            return false;
        }

        if (participant_->enable() != eprosima::fastrtps::types::ReturnCode_t::RETCODE_OK)
        {
            logError(DATABROKER_PARTICIPANT, "Fail to enable Participant " << name());
            return false;
        }
        enabled_ = true;
    }

    logInfo(DATABROKER_PARTICIPANT, "DataBroker Participant with name " << name() << " enabled");

    return true;
}

eprosima::fastdds::dds::DomainParticipantQos DataBrokerParticipant::participant_qos()
{
    eprosima::fastdds::dds::DomainParticipantQos participant_qos;

    // By default use UDPv4 due to communication failures between dockers sharing the network with the host
    // When it is solved in Fast-DDS delete the following lines and use the default builtin transport.
    participant_qos.transport().use_builtin_transports = false;
    auto udp_transport = std::make_shared<eprosima::fastdds::rtps::UDPv4TransportDescriptor>();
    participant_qos.transport().user_transports.push_back(udp_transport);

    return participant_qos;
}

void DataBrokerParticipant::add_topic(
        const std::string& topic_name)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    // Check if this topic already exists
    auto it = topics_.find(topic_name);

    if (it != topics_.end())
    {
        // Topic already exists
        return;
    }

    eprosima::fastdds::dds::Topic* topic = get_topic_(topic_name);

    if (!topic)
    {
        logError(DATABROKER_PARTICIPANT, "Error creating topic " << topic_name << " in Participant " << name());
        return;
    }

    // Create DataWriter
    eprosima::fastdds::dds::DataWriter* dw = publisher_->create_datawriter(
        topic,
        datawriter_qos());

    // Create DataReader
    eprosima::fastdds::dds::DataReader* dr = subscriber_->create_datareader(
        topic,
        datareader_qos());

    logInfo(DATABROKER_PARTICIPANT, "Topic '" << topic_name << "' created in Participant " << name());

    // Store new objects in maps
    topics_[topic_name] = topic;
    datawriters_[topic_name] = dw;
    datareaders_[topic_name] = dr;
}

void DataBrokerParticipant::stop_topic(
        const std::string& topic)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    // TODO
}

void DataBrokerParticipant::send_data(
        const std::string& topic,
        StdString& data)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    // Find correct DataWriter
    auto it = datawriters_.find(topic);
    if (it == datawriters_.end())
    {
        logError(DATABROKER_PARTICIPANT, "Datawriter missing for topic " << topic << " in Participant " << name());
        return;
    }

    eprosima::fastdds::dds::DataWriter* dw = it->second;

    dw->write(&data);

    logInfo(DATABROKER_PARTICIPANT, "Data sent in topic " << topic << " in Participant " << name());
}

eprosima::fastrtps::rtps::GuidPrefix_t DataBrokerParticipant::guid()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (participant_)
    {
        return participant_->guid().guidPrefix;
    }

    return eprosima::fastrtps::rtps::GUID_t().guidPrefix;
}

eprosima::fastdds::dds::DataWriterQos DataBrokerParticipant::datawriter_qos()
{
    eprosima::fastdds::dds::DataWriterQos datawriter_qos = eprosima::fastdds::dds::DATAWRITER_QOS_DEFAULT;

    datawriter_qos.publish_mode().kind = eprosima::fastdds::dds::PublishModeQosPolicyKind::ASYNCHRONOUS_PUBLISH_MODE;

    return datawriter_qos;
}

eprosima::fastdds::dds::DataReaderQos DataBrokerParticipant::datareader_qos()
{
    return eprosima::fastdds::dds::DATAREADER_QOS_DEFAULT;
}

bool DataBrokerParticipant::register_type_()
{
    // Register Type
    type_->setName(type_name_().c_str());
    return type_.register_type(participant_) == eprosima::fastrtps::types::ReturnCode_t::RETCODE_OK;
}

eprosima::fastdds::dds::Topic* DataBrokerParticipant::get_topic_(
        const std::string& topic_name)
{
    std::string topic_mangled = topic_mangled_(topic_name);

    logInfo(DATABROKER_PARTICIPANT, "Adding topic mangled '" << topic_mangled << "' endpoints for Participant "
                                                             << name());

    // Create Topic
    return participant_->create_topic(
        topic_mangled,
        type_name_(),
        eprosima::fastdds::dds::TOPIC_QOS_DEFAULT);
}

std::string DataBrokerParticipant::topic_mangled_(
        const std::string& topic_name)
{
    return topic_name;
}

std::string DataBrokerParticipant::type_name_()
{
    return "StdString";
}

std::string DataBrokerParticipant::name()
{
    return "DataBroker_Participant";
}

const DataBrokerParticipantConfiguration& DataBrokerParticipant::get_configuration_()
{
    return configuration_;
}

} /* namespace databroker */
} /* namespace eprosima */
