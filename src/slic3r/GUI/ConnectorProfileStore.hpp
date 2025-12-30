// Store and manage cut connector profiles.
#ifndef slic3r_GUI_ConnectorProfileStore_hpp_
#define slic3r_GUI_ConnectorProfileStore_hpp_

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "libslic3r/Model.hpp"

namespace Slic3r {

class AppConfig;

namespace GUI {

struct ConnectorProfileValues
{
    CutConnectorType type{ CutConnectorType::Plug };
    int              style{ int(CutConnectorStyle::Prism) };
    int              shape{ int(CutConnectorShape::Circle) };
    float            depth{ 3.f };
    float            depth_tolerance{ 0.1f };
    float            size{ 2.5f };
    float            size_tolerance{ 0.f };
    float            angle{ 0.f };
    float            snap_bulge_proportion{ 0.15f };
    float            snap_space_proportion{ 0.3f };
};

struct ConnectorProfile
{
    std::string            name;
    ConnectorProfileValues values;
};

class ConnectorProfileStore
{
public:
    static const std::string &default_profile_name();

    explicit ConnectorProfileStore(AppConfig *config = nullptr);

    void set_config(AppConfig *config);

    void load(const ConnectorProfile &fallback_profile);
    void persist() const;

    const ConnectorProfile              &active_profile() const { return m_active_profile; }
    const std::vector<ConnectorProfile> &profiles() const { return m_profiles; }
    std::vector<std::string>             profile_names() const;

    void update_active_values(const ConnectorProfileValues &values);
    void set_active_profile(const std::string &profile_name, const ConnectorProfile &fallback_profile);
    void save_profile(const std::string &profile_name, const ConnectorProfileValues &values);
    bool delete_profile(const std::string &profile_name, const ConnectorProfile &fallback_profile);

private:
    AppConfig *m_config{ nullptr };
    std::vector<ConnectorProfile> m_profiles;
    ConnectorProfile              m_active_profile;
    std::string                   m_last_active_name;

    static ConnectorProfileValues parsed_values(const nlohmann::json &values_json, const ConnectorProfileValues &fallback_values);

    ConnectorProfile build_profile(const std::string &profile_name, const ConnectorProfileValues &values) const;
    ConnectorProfile ensure_fallback(const ConnectorProfile &fallback_profile);
    void             ensure_active_present(const ConnectorProfile &fallback_profile);
    void             write_back() const;

    ConnectorProfile       *find_profile(const std::string &profile_name);
    const ConnectorProfile *find_profile(const std::string &profile_name) const;
    static CutConnectorType  sanitized_type(int raw_type, CutConnectorType fallback_type);
    static CutConnectorStyle sanitized_style(int raw_style, CutConnectorStyle fallback_style);
    static CutConnectorShape sanitized_shape(int raw_shape, CutConnectorShape fallback_shape);
};

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_GUI_ConnectorProfileStore_hpp_
