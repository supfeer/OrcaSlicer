#include "ConnectorProfileStore.hpp"

#include <algorithm>
#include <exception>

#include "libslic3r/AppConfig.hpp"

namespace Slic3r {
namespace GUI {

namespace {
constexpr auto CONNECTOR_SECTION = "cut_connectors";
constexpr auto CONNECTOR_PROFILES_KEY = "profiles";
constexpr auto LAST_PROFILE_KEY = "last_profile";
} // namespace

const std::string &ConnectorProfileStore::default_profile_name()
{
    static const std::string name = "Default";
    return name;
}

ConnectorProfileStore::ConnectorProfileStore(AppConfig *config)
    : m_config(config)
    , m_active_profile(build_profile(default_profile_name(), {}))
{
}

void ConnectorProfileStore::set_config(AppConfig *config)
{
    m_config = config;
}

void ConnectorProfileStore::load(const ConnectorProfile &fallback_profile)
{
    m_profiles.clear();
    m_active_profile = ensure_fallback(fallback_profile);
    m_last_active_name.clear();

    if (m_config == nullptr)
        return;

    const std::string profiles_json = m_config->get(CONNECTOR_SECTION, CONNECTOR_PROFILES_KEY);
    if (!profiles_json.empty()) {
        try {
            const auto parsed = nlohmann::json::parse(profiles_json);
            if (parsed.is_array()) {
                for (const auto &item : parsed) {
                    if (!item.contains("name") || !item.contains("values"))
                        continue;
                    const std::string profile_name = item["name"].get<std::string>();
                    const ConnectorProfileValues values = parsed_values(item["values"], fallback_profile.values);
                    if (find_profile(profile_name) == nullptr)
                        m_profiles.emplace_back(build_profile(profile_name, values));
                }
            }
        } catch (const std::exception &) {
        }
    }

    if (m_profiles.empty())
        m_profiles.push_back(fallback_profile);

    m_last_active_name = m_config->get(CONNECTOR_SECTION, LAST_PROFILE_KEY);
    if (!m_last_active_name.empty()) {
        if (const auto *stored = find_profile(m_last_active_name))
            m_active_profile = *stored;
    } else {
        m_active_profile = fallback_profile;
        m_last_active_name = fallback_profile.name;
    }

    ensure_active_present(fallback_profile);
}

void ConnectorProfileStore::persist() const
{
    if (m_config == nullptr)
        return;

    write_back();
}

std::vector<std::string> ConnectorProfileStore::profile_names() const
{
    std::vector<std::string> names;
    names.reserve(m_profiles.size());
    for (const auto &profile : m_profiles)
        names.push_back(profile.name);
    return names;
}

void ConnectorProfileStore::update_active_values(const ConnectorProfileValues &values)
{
    m_active_profile.values = values;
    auto *existing = find_profile(m_active_profile.name);
    if (existing)
        existing->values = values;
    else
        m_profiles.push_back(m_active_profile);

    m_last_active_name = m_active_profile.name;
    persist();
}

void ConnectorProfileStore::set_active_profile(const std::string &profile_name, const ConnectorProfile &fallback_profile)
{
    if (profile_name.empty())
        return;

    const ConnectorProfile *found = find_profile(profile_name);
    m_active_profile = found != nullptr ? *found : fallback_profile;
    m_last_active_name = m_active_profile.name;
    ensure_active_present(fallback_profile);
    persist();
}

void ConnectorProfileStore::save_profile(const std::string &profile_name, const ConnectorProfileValues &values)
{
    if (profile_name.empty())
        return;

    m_active_profile = build_profile(profile_name, values);
    if (auto *existing = find_profile(profile_name))
        *existing = m_active_profile;
    else
        m_profiles.push_back(m_active_profile);

    m_last_active_name = profile_name;
    persist();
}

bool ConnectorProfileStore::delete_profile(const std::string &profile_name, const ConnectorProfile &fallback_profile)
{
    if (profile_name.empty())
        return false;

    if (profile_name == fallback_profile.name)
        return false;

    const auto it = std::remove_if(m_profiles.begin(), m_profiles.end(), [&profile_name](const ConnectorProfile &profile) { return profile.name == profile_name; });
    if (it == m_profiles.end())
        return false;

    m_profiles.erase(it, m_profiles.end());
    if (m_profiles.empty())
        m_profiles.push_back(fallback_profile);

    if (m_last_active_name == profile_name)
        m_last_active_name = m_profiles.front().name;

    if (m_active_profile.name == profile_name) {
        m_active_profile = ensure_fallback(fallback_profile);
        ensure_active_present(fallback_profile);
    }

    persist();
    return true;
}

ConnectorProfileValues ConnectorProfileStore::parsed_values(const nlohmann::json &values_json, const ConnectorProfileValues &fallback_values)
{
    ConnectorProfileValues values = fallback_values;
    values.type                   = sanitized_type(values_json.value("type", int(fallback_values.type)), fallback_values.type);
    values.style                  = values_json.value("style", fallback_values.style);
    values.shape                  = values_json.value("shape", fallback_values.shape);
    values.depth                  = values_json.value("depth", fallback_values.depth);
    values.depth_tolerance        = values_json.value("depth_tolerance", fallback_values.depth_tolerance);
    values.size                   = values_json.value("size", fallback_values.size);
    values.size_tolerance         = values_json.value("size_tolerance", fallback_values.size_tolerance);
    values.angle                  = values_json.value("angle", fallback_values.angle);
    values.snap_bulge_proportion  = values_json.value("snap_bulge_proportion", fallback_values.snap_bulge_proportion);
    values.snap_space_proportion  = values_json.value("snap_space_proportion", fallback_values.snap_space_proportion);

    values.style = int(sanitized_style(values.style, CutConnectorStyle(fallback_values.style)));
    values.shape = int(sanitized_shape(values.shape, CutConnectorShape(fallback_values.shape)));
    return values;
}

ConnectorProfile ConnectorProfileStore::build_profile(const std::string &profile_name, const ConnectorProfileValues &values) const
{
    return ConnectorProfile{ profile_name.empty() ? default_profile_name() : profile_name, values };
}

ConnectorProfile ConnectorProfileStore::ensure_fallback(const ConnectorProfile &fallback_profile)
{
    ConnectorProfile ensured = fallback_profile.name.empty() ? build_profile(default_profile_name(), fallback_profile.values) : fallback_profile;
    if (find_profile(ensured.name) == nullptr)
        m_profiles.push_back(ensured);
    return ensured;
}

void ConnectorProfileStore::ensure_active_present(const ConnectorProfile &fallback_profile)
{
    if (find_profile(m_active_profile.name) == nullptr)
        m_profiles.push_back(m_active_profile);

    if (m_active_profile.name.empty())
        m_active_profile = fallback_profile;
}

void ConnectorProfileStore::write_back() const
{
    nlohmann::json profiles_json = nlohmann::json::array();
    for (const auto &profile : m_profiles) {
        nlohmann::json values_json;
        values_json["type"]                  = int(profile.values.type);
        values_json["style"]                 = profile.values.style;
        values_json["shape"]                 = profile.values.shape;
        values_json["depth"]                 = profile.values.depth;
        values_json["depth_tolerance"]       = profile.values.depth_tolerance;
        values_json["size"]                  = profile.values.size;
        values_json["size_tolerance"]        = profile.values.size_tolerance;
        values_json["angle"]                 = profile.values.angle;
        values_json["snap_bulge_proportion"] = profile.values.snap_bulge_proportion;
        values_json["snap_space_proportion"] = profile.values.snap_space_proportion;

        profiles_json.push_back(nlohmann::json{ { "name", profile.name }, { "values", values_json } });
    }

    m_config->set(CONNECTOR_SECTION, CONNECTOR_PROFILES_KEY, profiles_json.dump());
    m_config->set(CONNECTOR_SECTION, LAST_PROFILE_KEY, m_last_active_name);
}

ConnectorProfile *ConnectorProfileStore::find_profile(const std::string &profile_name)
{
    const auto it = std::find_if(m_profiles.begin(), m_profiles.end(), [&profile_name](const ConnectorProfile &profile) { return profile.name == profile_name; });
    return it != m_profiles.end() ? &(*it) : nullptr;
}

const ConnectorProfile *ConnectorProfileStore::find_profile(const std::string &profile_name) const
{
    const auto it = std::find_if(m_profiles.begin(), m_profiles.end(), [&profile_name](const ConnectorProfile &profile) { return profile.name == profile_name; });
    return it != m_profiles.end() ? &(*it) : nullptr;
}

CutConnectorType ConnectorProfileStore::sanitized_type(int raw_type, CutConnectorType fallback_type)
{
    switch (CutConnectorType(raw_type)) {
    case CutConnectorType::Plug:
    case CutConnectorType::Dowel:
    case CutConnectorType::Snap:
        return CutConnectorType(raw_type);
    default:
        return fallback_type;
    }
}

CutConnectorStyle ConnectorProfileStore::sanitized_style(int raw_style, CutConnectorStyle fallback_style)
{
    switch (CutConnectorStyle(raw_style)) {
    case CutConnectorStyle::Prism:
    case CutConnectorStyle::Frustum:
        return CutConnectorStyle(raw_style);
    default:
        return fallback_style;
    }
}

CutConnectorShape ConnectorProfileStore::sanitized_shape(int raw_shape, CutConnectorShape fallback_shape)
{
    switch (CutConnectorShape(raw_shape)) {
    case CutConnectorShape::Triangle:
    case CutConnectorShape::Square:
    case CutConnectorShape::Hexagon:
    case CutConnectorShape::Circle:
        return CutConnectorShape(raw_shape);
    default:
        return fallback_shape;
    }
}

} // namespace GUI
} // namespace Slic3r
