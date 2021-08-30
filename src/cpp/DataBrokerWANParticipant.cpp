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

#include <fastdds/rtps/transport/UDPv4TransportDescriptor.h>
#include <fastdds/rtps/transport/TCPv4TransportDescriptor.h>

#include <databroker/DataBrokerWANParticipant.hpp>
#include <databroker/Address.hpp>


namespace eprosima {
namespace databroker {

DataBrokerWANParticipant::DataBrokerWANParticipant(
        eprosima::fastdds::dds::DomainParticipantListener* listener,
        eprosima::fastrtps::rtps::GuidPrefix_t server_guid,
        uint32_t domain /* = 0 */,
        std::string name /* = "DataBroker Participant" */)
    : DataBrokerParticipant(listener, domain, name)
    , guid_(server_guid)
{
}

eprosima::fastrtps::rtps::GuidPrefix_t DataBrokerWANParticipant::guid()
{
    return guid_;
}

// TODO add debug traces
eprosima::fastdds::dds::DomainParticipantQos DataBrokerWANParticipant::wan_participant_qos(
        const eprosima::fastrtps::rtps::GuidPrefix_t& server_guid,
        const std::vector<Address>& listening_addresses,
        const std::vector<Address>& connection_addresses,
        const bool& udp)
{
    eprosima::fastdds::dds::DomainParticipantQos pqos = default_participant_qos();

    // Configuring Server Guid
    pqos.wire_protocol().prefix = server_guid;

    logInfo(DATABROKER, "External Discovery Server set with guid " << server_guid);

    for (auto address : listening_addresses)
    {
        // Configuring transport
        if (!udp)
        {
            // In case of using TCP, configure listening address
            // Create TCPv4 transport
            std::shared_ptr<eprosima::fastdds::rtps::TCPv4TransportDescriptor> descriptor =
                    std::make_shared<eprosima::fastdds::rtps::TCPv4TransportDescriptor>();

            descriptor->add_listener_port(address.port);
            descriptor->set_WAN_address(address.ip);

            descriptor->sendBufferSize = 0;
            descriptor->receiveBufferSize = 0;

            pqos.transport().user_transports.push_back(descriptor);

            logInfo(DATABROKER, "External Discovery Server configure TCP listening address " << address);
        }

        // Create Locator
        eprosima::fastrtps::rtps::Locator_t locator;
        locator.kind = udp ? LOCATOR_KIND_UDPv4 : LOCATOR_KIND_TCPv4;

        eprosima::fastrtps::rtps::IPLocator::setIPv4(locator, address.ip);
        eprosima::fastrtps::rtps::IPLocator::setWan(locator, address.ip);
        eprosima::fastrtps::rtps::IPLocator::setLogicalPort(locator, address.port);
        eprosima::fastrtps::rtps::IPLocator::setPhysicalPort(locator, address.port);

        pqos.wire_protocol().builtin.metatrafficUnicastLocatorList.push_back(locator);

        logInfo(DATABROKER, "External Discovery Server configure listening locator " << locator);
    }

    // Configure connection addresses
    for (auto address : connection_addresses)
    {
        eprosima::fastrtps::rtps::RemoteServerAttributes server_attr;

        // Set Server GUID
        server_attr.guidPrefix = address.guid;

        // Discovery server locator configuration TCP
        eprosima::fastrtps::rtps::Locator_t locator;
        locator.kind = udp ? LOCATOR_KIND_UDPv4 : LOCATOR_KIND_TCPv4;

        eprosima::fastrtps::rtps::IPLocator::setIPv4(locator, address.ip);
        eprosima::fastrtps::rtps::IPLocator::setLogicalPort(locator, address.port);
        eprosima::fastrtps::rtps::IPLocator::setPhysicalPort(locator, address.port);
        server_attr.metatrafficUnicastLocatorList.push_back(locator);

        pqos.wire_protocol().builtin.discovery_config.m_DiscoveryServers.push_back(server_attr);

        logInfo(DATABROKER, "External Discovery Server configure connection locator " << locator
                                                                                      << " to server " <<
                server_attr.guidPrefix);
    }

    // TODO decide the discovery server configuration
    pqos.wire_protocol().builtin.discovery_config.leaseDuration = fastrtps::c_TimeInfinite;
    pqos.wire_protocol().builtin.discovery_config.leaseDuration_announcementperiod =
            fastrtps::Duration_t(2, 0);

    // Set this participant as a SERVER
    pqos.wire_protocol().builtin.discovery_config.discoveryProtocol =
            fastrtps::rtps::DiscoveryProtocol::SERVER;

    return pqos;
}

eprosima::fastdds::dds::DataWriterQos DataBrokerWANParticipant::default_datawriter_qos()
{
    eprosima::fastdds::dds::DataWriterQos datawriter_qos = DataBrokerParticipant::default_datawriter_qos();

    datawriter_qos.publish_mode().kind = eprosima::fastdds::dds::PublishModeQosPolicyKind::ASYNCHRONOUS_PUBLISH_MODE;
    datawriter_qos.reliability().kind = eprosima::fastdds::dds::ReliabilityQosPolicyKind::RELIABLE_RELIABILITY_QOS;
    datawriter_qos.durability().kind = eprosima::fastdds::dds::DurabilityQosPolicyKind::TRANSIENT_LOCAL_DURABILITY_QOS;

    return datawriter_qos;
}

eprosima::fastdds::dds::DataReaderQos DataBrokerWANParticipant::default_datareader_qos()
{
    eprosima::fastdds::dds::DataReaderQos datareader_qos = DataBrokerParticipant::default_datareader_qos();

    datareader_qos.reliability().kind = eprosima::fastdds::dds::ReliabilityQosPolicyKind::RELIABLE_RELIABILITY_QOS;
    datareader_qos.durability().kind = eprosima::fastdds::dds::DurabilityQosPolicyKind::TRANSIENT_LOCAL_DURABILITY_QOS;

    return datareader_qos;
}

} /* namespace databroker */
} /* namespace eprosima */