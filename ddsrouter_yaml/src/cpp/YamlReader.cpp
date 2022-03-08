// Copyright 2022 Proyectos y Sistemas de Mantenimiento SL (eProsima).
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
 * @file YamlReader.cpp
 *
 */

#include <ddsrouter_core/configuration/participant/DiscoveryServerParticipantConfiguration.hpp>
#include <ddsrouter_core/configuration/participant/ParticipantConfiguration.hpp>
#include <ddsrouter_core/configuration/participant/SimpleParticipantConfiguration.hpp>
#include <ddsrouter_core/types/address/Address.hpp>
#include <ddsrouter_core/types/address/DiscoveryServerConnectionAddress.hpp>
#include <ddsrouter_core/types/dds/DomainId.hpp>
#include <ddsrouter_core/types/dds/GuidPrefix.hpp>
#include <ddsrouter_core/types/participant/ParticipantId.hpp>
#include <ddsrouter_core/types/participant/ParticipantKind.hpp>
#include <ddsrouter_core/types/security/tls/TlsConfiguration.hpp>
#include <ddsrouter_core/types/security/tls/TlsConfigurationBoth.hpp>
#include <ddsrouter_core/types/security/tls/TlsConfigurationClient.hpp>
#include <ddsrouter_core/types/security/tls/TlsConfigurationServer.hpp>
#include <ddsrouter_core/types/topic/RealTopic.hpp>
#include <ddsrouter_core/types/topic/WildcardTopic.hpp>
#include <ddsrouter_utils/Log.hpp>
#include <ddsrouter_utils/utils.hpp>

#include <ddsrouter_yaml/Yaml.hpp>
#include <ddsrouter_yaml/YamlReader.hpp>
#include <ddsrouter_yaml/yaml_configuration_tags.hpp>

namespace eprosima {
namespace ddsrouter {
namespace yaml {

using namespace eprosima::ddsrouter::core;
using namespace eprosima::ddsrouter::core::types;

/************************
* GENERIC              *
************************/

bool YamlReader::is_tag_present(
        const Yaml& yml,
        const TagType& tag)
{
    if (!yml.IsMap() && !yml.IsNull())
    {
        throw utils::ConfigurationException(
                  utils::Formatter() << "Trying to find a tag: <" << tag << "> in a not yaml object map.");
    }

    // Explicit conversion to avoid windows format warning
    // This method performace is the same as only retrieving bool
    return (yml[tag]) ? true : false;
}

Yaml YamlReader::get_value_in_tag(
        const Yaml& yml,
        const TagType& tag)
{
    if (is_tag_present(yml, tag))
    {
        return yml[tag];
    }
    else
    {
        throw utils::ConfigurationException(
                  utils::Formatter() << "Required tag not found: <" << tag << ">.");
    }
}

/************************
* ENTITIES             *
************************/

template <>
TransportProtocol YamlReader::get<TransportProtocol>(
        const Yaml& yml)
{
    return get_enumeration<TransportProtocol>(
        yml,
                {
                    {ADDRESS_TRANSPORT_TCP_TAG, TransportProtocol::TCP},
                    {ADDRESS_TRANSPORT_UDP_TAG, TransportProtocol::UDP},
                });
}

template <>
IpVersion YamlReader::get<IpVersion>(
        const Yaml& yml)
{
    return get_enumeration<IpVersion>(
        yml,
                {
                    {ADDRESS_IP_VERSION_V4_TAG, IpVersion::IPv4},
                    {ADDRESS_IP_VERSION_V6_TAG, IpVersion::IPv6},
                });
}

template <>
PortType YamlReader::get<PortType>(
        const Yaml& yml)
{
    // Domain id required
    return PortType(get_scalar<PortType>(yml));
}

template <>
IpType YamlReader::get<IpType>(
        const Yaml& yml)
{
    // Domain id required
    return IpType(get_scalar<IpType>(yml));
}

template <>
ParticipantId YamlReader::get<ParticipantId>(
        const Yaml& yml)
{
    // Participant name required
    return ParticipantId(get_scalar<std::string>(yml));
}

template <>
ParticipantKind YamlReader::get<ParticipantKind>(
        const Yaml& yml)
{
    // Participant kind required
    return ParticipantKind::participant_kind_from_name(get_scalar<std::string>(yml));
}

template <>
DomainId YamlReader::get<DomainId>(
        const Yaml& yml)
{
    // Domain id required
    return DomainId(get_scalar<DomainIdType>(yml));
}

template <>
GuidPrefix YamlReader::get<GuidPrefix>(
        const Yaml& yml)
{
    // If guid exists, use it. Non mandatory.
    if (is_tag_present(yml, DISCOVERY_SERVER_GUID_TAG))
    {
        std::string guid = get_scalar<std::string>(yml, DISCOVERY_SERVER_GUID_TAG);
        return GuidPrefix(guid);
    }

    // ROS DS is optional.
    bool ros_id;
    bool ros_id_set = is_tag_present(yml, DISCOVERY_SERVER_ID_ROS_TAG);
    if (ros_id_set)
    {
        ros_id = get_scalar<bool>(yml, DISCOVERY_SERVER_ID_ROS_TAG);
    }

    // Id is mandatory if guid is not present
    uint32_t id = get_scalar<uint32_t>(yml, DISCOVERY_SERVER_ID_TAG);

    // Create GuidPrefix
    if (ros_id_set)
    {
        return GuidPrefix(ros_id, id);
    }
    else
    {
        return GuidPrefix(id);
    }
}

template <>
Address YamlReader::get<Address>(
        const Yaml& yml)
{
    // Optional get IP version
    IpVersion ip_version;
    bool ip_version_set = is_tag_present(yml, ADDRESS_IP_VERSION_TAG);
    if (ip_version_set)
    {
        // Get IP Version from enumeration
        ip_version = get<IpVersion>(yml, ADDRESS_IP_VERSION_TAG);
    }

    // Optional get IP
    IpType ip;
    bool ip_set = is_tag_present(yml, ADDRESS_IP_TAG);
    if (ip_set)
    {
        ip = get<IpType>(yml, ADDRESS_IP_TAG);
    }

    // Optional get Domain tag for DNS
    std::string domain_name;
    bool domain_name_set = is_tag_present(yml, ADDRESS_DNS_TAG);
    if (domain_name_set)
    {
        domain_name = get<std::string>(yml, ADDRESS_DNS_TAG);
    }

    // If IP and domain_name set, warning that domain_name will not be used
    // If only domain_name set, get DNS response
    // If neither set, get default
    if (ip_set && domain_name_set)
    {
        logWarning(DDSROUTER_YAML,
                "Tag <" << ADDRESS_DNS_TAG << "> will not be used as <" << ADDRESS_IP_TAG << "> is set.");
        domain_name_set = false;
    }
    else if (!ip_set && !domain_name_set)
    {
        throw utils::ConfigurationException(utils::Formatter() <<
                      "Address requires to specify <" << ADDRESS_IP_TAG << "> or <" << ADDRESS_DNS_TAG << ">.");
    }

    // Optional get port
    PortType port;
    bool port_set = is_tag_present(yml, ADDRESS_PORT_TAG);
    if (port_set)
    {
        port = get<PortType>(yml, ADDRESS_PORT_TAG);
    }
    else
    {
        port = Address::default_port();
    }

    // Optional get Transport protocol
    TransportProtocol tp;
    bool tp_set = is_tag_present(yml, ADDRESS_TRANSPORT_TAG);
    if (tp_set)
    {
        tp = get<TransportProtocol>(yml, ADDRESS_TRANSPORT_TAG);
    }
    else
    {
        tp = Address::default_transport_protocol();
    }

    // Construct Address object
    if (domain_name_set)
    {
        if (ip_version_set)
        {
            return Address(port, ip_version, domain_name, tp);
        }
        else
        {
            return Address(port, domain_name, tp);
        }
    }
    else
    {
        if (ip_version_set)
        {
            return Address(ip, port, ip_version, tp);
        }
        else
        {
            return Address(ip, port, tp);
        }
    }
}

template <>
DiscoveryServerConnectionAddress YamlReader::get<DiscoveryServerConnectionAddress>(
        const Yaml& yml)
{
    // GuidPrefix required
    GuidPrefix server_guid = get<GuidPrefix>(yml, DISCOVERY_SERVER_GUID_PREFIX_TAG);

    // Addresses required
    std::set<Address> addresses = get_set<Address>(yml, COLLECTION_ADDRESSES_TAG);

    // Create Connection Address
    return DiscoveryServerConnectionAddress(server_guid, addresses);
}

template <>
RealTopic YamlReader::get<RealTopic>(
        const Yaml& yml)
{
    // Mandatory name
    std::string name = get_scalar<std::string>(yml, TOPIC_NAME_TAG);

    // Mandatory type
    std::string type = get_scalar<std::string>(yml, TOPIC_TYPE_NAME_TAG);

    // Optional keyed
    bool keyed;
    bool keyed_set = is_tag_present(yml, TOPIC_KIND_TAG);
    if (keyed_set)
    {
        keyed = get_scalar<bool>(yml, TOPIC_KIND_TAG);
    }

    // Create Topic
    if (keyed_set)
    {
        return RealTopic(name, type, keyed);
    }
    else
    {
        return RealTopic(name, type);
    }
}

template <>
WildcardTopic YamlReader::get<WildcardTopic>(
        const Yaml& yml)
{
    // Mandatory name
    std::string name = get_scalar<std::string>(yml, TOPIC_NAME_TAG);

    // Optional type
    std::string type;
    bool type_set = is_tag_present(yml, TOPIC_TYPE_NAME_TAG);
    if (type_set)
    {
        type = get_scalar<std::string>(yml, TOPIC_TYPE_NAME_TAG);
    }

    // Optional keyed
    bool keyed;
    bool keyed_set = is_tag_present(yml, TOPIC_KIND_TAG);
    if (keyed_set)
    {
        keyed = get_scalar<bool>(yml, TOPIC_KIND_TAG);
    }

    // Create Topic
    if (keyed_set)
    {
        if (type_set)
        {
            return WildcardTopic(name, type, true, keyed);
        }
        else
        {
            return WildcardTopic(name, true, keyed);
        }
    }
    else
    {
        if (type_set)
        {
            return WildcardTopic(name, type, false);
        }
        else
        {
            return WildcardTopic(name, false);
        }
    }
}

template <>
std::shared_ptr<security::TlsConfiguration> YamlReader::get<std::shared_ptr<security::TlsConfiguration>>(
        const Yaml& yml)
{
    // Optional private key
    std::string private_key_file;
    bool has_private_key_file = is_tag_present(yml, TLS_PRIVATE_KEY_TAG);
    if (has_private_key_file)
    {
        private_key_file = get_scalar<std::string>(yml, TLS_PRIVATE_KEY_TAG);
    }

    // Optional private key password
    std::string private_key_file_password;
    bool has_private_key_file_password = is_tag_present(yml, TLS_PASSWORD_TAG);
    if (has_private_key_file_password)
    {
        private_key_file_password = get_scalar<std::string>(yml, TLS_PASSWORD_TAG);
    }

    // Optional certificate authority
    std::string certificate_authority_file;
    bool has_certificate_authority_file = is_tag_present(yml, TLS_CA_TAG);
    if (has_certificate_authority_file)
    {
        certificate_authority_file = get_scalar<std::string>(yml, TLS_CA_TAG);
    }

    // Optional certificate chain
    std::string certificate_chain_file;
    bool has_certificate_chain_file = is_tag_present(yml, TLS_CERT_TAG);
    if (has_certificate_chain_file)
    {
        certificate_chain_file = get_scalar<std::string>(yml, TLS_CERT_TAG);
    }

    // Optional dh params
    std::string dh_params_file;
    bool has_dh_params_file = is_tag_present(yml, TLS_DHPARAMS_TAG);
    if (has_dh_params_file)
    {
        dh_params_file = get_scalar<std::string>(yml, TLS_DHPARAMS_TAG);
    }

    if (has_private_key_file && has_certificate_chain_file && has_dh_params_file)
    {
        if (has_certificate_authority_file)
        {
            // Both TLS configuration
            return std::make_shared<security::TlsConfigurationBoth>(
                private_key_file_password,
                private_key_file,
                certificate_authority_file,
                certificate_chain_file,
                dh_params_file);
        }
        else
        {
            // Server TLS configuration
            return std::make_shared<security::TlsConfigurationServer>(
                private_key_file_password,
                private_key_file,
                certificate_chain_file,
                dh_params_file);
        }
    }
    else
    {
        if (has_certificate_authority_file)
        {
            // Client TLS configuration
            return std::make_shared<security::TlsConfigurationClient>(certificate_authority_file);
        }
        else
        {
            throw utils::ConfigurationException(
                      "TLS Configuration is set and does not fit with Client or Server parameters.");
        }
    }
}

/************************
* PARTICIPANTS         *
************************/

template <>
configuration::ParticipantConfiguration YamlReader::get<configuration::ParticipantConfiguration>(
        const Yaml& yml)
{
    // Id required
    types::ParticipantId id = get<types::ParticipantId>(yml, PARTICIPANT_NAME_TAG);

    // Kind required
    types::ParticipantKind kind = get<types::ParticipantKind>(yml, PARTICIPANT_KIND_TAG);

    return configuration::ParticipantConfiguration(id, kind);
}

template <>
configuration::SimpleParticipantConfiguration YamlReader::get<configuration::SimpleParticipantConfiguration>(
        const Yaml& yml)
{
    // Id required
    types::ParticipantId id = get<types::ParticipantId>(yml, PARTICIPANT_NAME_TAG);

    // Kind required
    types::ParticipantKind kind = get<types::ParticipantKind>(yml, PARTICIPANT_KIND_TAG);

    // Domain required
    types::DomainId domain = get<types::DomainId>(yml, DOMAIN_ID_TAG);

    return configuration::SimpleParticipantConfiguration(id, kind, domain);
}

template <>
configuration::DiscoveryServerParticipantConfiguration YamlReader::get<configuration::DiscoveryServerParticipantConfiguration>(
        const Yaml& yml)
{
    // Id required
    types::ParticipantId id = get<types::ParticipantId>(yml, PARTICIPANT_NAME_TAG);

    // Kind required
    types::ParticipantKind kind = get<types::ParticipantKind>(yml, PARTICIPANT_KIND_TAG);

    // Guid Prefix required
    types::GuidPrefix guid = get<types::GuidPrefix>(yml, DISCOVERY_SERVER_GUID_PREFIX_TAG);

    // Domain option
    types::DomainId domain;
    bool has_domain = is_tag_present(yml, DOMAIN_ID_TAG);
    if (has_domain)
    {
        domain = get<types::DomainId>(yml, DOMAIN_ID_TAG);
    }

    // Optional listening addresses
    std::set<types::Address> listening_addresses;
    if (is_tag_present(yml, LISTENING_ADDRESSES_TAG))
    {
        listening_addresses = get_set<types::Address>(yml, LISTENING_ADDRESSES_TAG);
    }

    // Optional connection addresses
    std::set<types::DiscoveryServerConnectionAddress> connection_addresses;
    if (is_tag_present(yml, CONNECTION_ADDRESSES_TAG))
    {
        connection_addresses = get_set<types::DiscoveryServerConnectionAddress>(yml, CONNECTION_ADDRESSES_TAG);
    }

    // Optional TLS
    std::shared_ptr<types::security::TlsConfiguration> tls;
    bool has_tls = is_tag_present(yml, TLS_TAG);
    if (has_tls)
    {
        tls = get<std::shared_ptr<types::security::TlsConfiguration>>(yml, TLS_TAG);
    }

    if (has_domain)
    {
        if (has_tls)
        {
            return configuration::DiscoveryServerParticipantConfiguration(
                id,
                guid,
                listening_addresses,
                connection_addresses,
                kind,
                tls,
                domain);
        }
        else
        {
            return configuration::DiscoveryServerParticipantConfiguration(
                id,
                guid,
                listening_addresses,
                connection_addresses,
                domain,
                kind);
        }
    }
    else
    {
        if (has_tls)
        {
            return configuration::DiscoveryServerParticipantConfiguration(
                id,
                guid,
                listening_addresses,
                connection_addresses,
                kind,
                tls);
        }
        else
        {
            return configuration::DiscoveryServerParticipantConfiguration(
                id,
                guid,
                listening_addresses,
                connection_addresses,
                kind);
        }
    }
}

} /* namespace yaml */
} /* namespace ddsrouter */
} /* namespace eprosima */