#include "protocol/genicam_xml_generator.hpp"

#include <algorithm>
#include <sstream>
#include <string_view>

namespace camera::protocol {
namespace {

std::string access_name(core::RegisterAccess access) {
    switch (access) {
    case core::RegisterAccess::ReadOnly: return "RO";
    case core::RegisterAccess::ReadWrite: return "RW";
    case core::RegisterAccess::WriteOnly: return "WO";
    }
    return "RO";
}

std::string hex_address(std::uint32_t address) {
    std::ostringstream out;
    out << "0x" << std::hex << address;
    return out.str();
}

std::string escape(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char ch : text) {
        switch (ch) {
        case '&': result += "&amp;"; break;
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '"': result += "&quot;"; break;
        case '\'': result += "&apos;"; break;
        default: result += ch; break;
        }
    }
    return result;
}

const core::RegisterSpec* find_register(const core::RegisterMap& map, std::uint32_t address) {
    for (const auto& item : map.registers()) {
        if (item.address == address) return &item;
    }
    return nullptr;
}

void emit_category(std::ostringstream& xml, std::string_view name,
                   const std::vector<const core::FeatureSpec*>& features) {
    xml << "  <Category Name=\"" << name << "\" NameSpace=\"Standard\">\n";
    for (const auto* feature : features) {
        xml << "    <pFeature>" << escape(feature->name) << "</pFeature>\n";
    }
    xml << "  </Category>\n";
}

void emit_register(std::ostringstream& xml, const core::RegisterSpec& reg,
                   std::string_view nodeName, bool asString) {
    const auto kind = asString ? "StringReg" : "IntReg";
    const auto registerNodeName = std::string(nodeName).ends_with("Register")
        ? std::string(nodeName) : std::string(nodeName) + "Register";
    xml << "  <" << kind << " Name=\"" << escape(registerNodeName) << "\" NameSpace=\"Custom\">\n"
        << "    <Address>" << hex_address(reg.address) << "</Address>\n"
        << "    <Length>" << reg.length << "</Length>\n"
        << "    <AccessMode>" << access_name(reg.access) << "</AccessMode>\n"
        << "    <pPort>Device</pPort>\n";
    if (!asString) {
        xml << "    <Sign>Unsigned</Sign>\n"
            << "    <Endianess>BigEndian</Endianess>\n";
    }
    xml << "  </" << kind << ">\n";
}

void emit_feature(std::ostringstream& xml, const core::FeatureSpec& feature,
                  const core::RegisterMap& map) {
    const auto name = escape(feature.name);
    if (feature.kind == core::FeatureKind::String) {
        xml << "  <String Name=\"" << name << "\" NameSpace=\"Standard\">\n"
            << "    <DisplayName>" << escape(feature.displayName) << "</DisplayName>\n"
            << "    <pValue>" << feature.name << "Register</pValue>\n"
            << "  </String>\n";
        if (const auto* reg = find_register(map, feature.address)) emit_register(xml, *reg, feature.name, true);
        return;
    }

    const char* nodeType = "Integer";
    switch (feature.kind) {
    case core::FeatureKind::Enumeration: nodeType = "Enumeration"; break;
    case core::FeatureKind::Command: nodeType = "Command"; break;
    case core::FeatureKind::Boolean: nodeType = "Boolean"; break;
    default: break;
    }
    xml << "  <" << nodeType << " Name=\"" << name << "\" NameSpace=\"Standard\">\n"
        << "    <DisplayName>" << escape(feature.displayName) << "</DisplayName>\n";
    if (!feature.description.empty()) xml << "    <Description>" << escape(feature.description) << "</Description>\n";
    if (feature.kind == core::FeatureKind::Enumeration) {
        for (const auto& entry : feature.enumEntries) {
            xml << "    <EnumEntry Name=\"" << escape(entry.name) << "\" NameSpace=\"Standard\">\n"
                << "      <Value>" << entry.value << "</Value>\n"
                << "    </EnumEntry>\n";
        }
    }
    if (feature.kind == core::FeatureKind::Command) {
        const std::string registerName = feature.address == 0x124 ? "AcquisitionCommandRegister" : feature.name + "Register";
        xml << "    <pValue>" << registerName << "</pValue>\n"
            << "    <CommandValue>" << feature.commandValue << "</CommandValue>\n"
            << "  </Command>\n";
        if (feature.name == "AcquisitionStart") {
            if (const auto* reg = find_register(map, feature.address)) emit_register(xml, *reg, registerName, false);
        } else if (feature.name != "AcquisitionStop") {
            if (const auto* reg = find_register(map, feature.address)) emit_register(xml, *reg, registerName, false);
        }
        return;
    }
    xml << "    <pValue>" << name << "Register</pValue>\n";
    if (feature.kind == core::FeatureKind::Integer) {
        xml << "    <Min>" << feature.minimum << "</Min>\n"
            << "    <Max>" << feature.maximum << "</Max>\n"
            << "    <Inc>" << feature.increment << "</Inc>\n";
    }
    xml << "  </" << nodeType << ">\n";
    if (const auto* reg = find_register(map, feature.address)) emit_register(xml, *reg, feature.name, false);
}

} // namespace

std::string GenicamXmlGenerator::generate(const core::RegisterMap& registerMap) {
    std::ostringstream xml;
    const auto& config = registerMap.config();
    xml << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        << "<RegisterDescription ModelName=\"" << escape(config.model) << "\" VendorName=\""
        << escape(config.vendor) << "\" StandardNameSpace=\"None\" SchemaMajorVersion=\"1\""
        << " SchemaMinorVersion=\"1\" SchemaSubMinorVersion=\"0\" MajorVersion=\"1\""
        << " MinorVersion=\"0\" SubMinorVersion=\"0\" ToolTip=\"GigE Vision camera simulator\""
        << " ProductGuid=\"00000000-0000-0000-0000-000000000001\""
        << " VersionGuid=\"00000000-0000-0000-0000-000000000001\""
        << " xmlns=\"http://www.genicam.org/GenApi/Version_1_1\""
        << " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
        << " xsi:schemaLocation=\"http://www.genicam.org/GenApi/Version_1_1 GenApiSchema.xsd\">\n";

    std::vector<const core::FeatureSpec*> device;
    std::vector<const core::FeatureSpec*> image;
    std::vector<const core::FeatureSpec*> acquisition;
    std::vector<const core::FeatureSpec*> transport;
    for (const auto& feature : registerMap.features()) {
        auto* target = &device;
        if (feature.category == "ImageFormatControl") target = &image;
        else if (feature.category == "AcquisitionControl") target = &acquisition;
        else if (feature.category == "TransportLayerControl") target = &transport;
        target->push_back(&feature);
    }

    xml << "  <Category Name=\"Root\" NameSpace=\"Standard\">\n"
        << "    <pFeature>DeviceControl</pFeature>\n"
        << "    <pFeature>ImageFormatControl</pFeature>\n"
        << "    <pFeature>AcquisitionControl</pFeature>\n"
        << "    <pFeature>TransportLayerControl</pFeature>\n"
        << "  </Category>\n";
    emit_category(xml, "DeviceControl", device);
    emit_category(xml, "ImageFormatControl", image);
    emit_category(xml, "AcquisitionControl", acquisition);
    emit_category(xml, "TransportLayerControl", transport);

    for (const auto& feature : registerMap.features()) emit_feature(xml, feature, registerMap);
    xml << "  <Port Name=\"Device\" NameSpace=\"Standard\"/>\n"
        << "</RegisterDescription>\n";
    return xml.str();
}

} // namespace camera::protocol
