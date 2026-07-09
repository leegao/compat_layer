#!/usr/bin/env python3
import json
import sys
import os

PROMOTED_EXTENSIONS = {
    "VK_KHR_maintenance1": (1, 1),
    "VK_KHR_maintenance2": (1, 1),
    "VK_KHR_shader_draw_parameters": (1, 1),
    "VK_KHR_dedicated_allocation": (1, 1),
    "VK_KHR_get_memory_requirements2": (1, 1),
    "VK_KHR_get_physical_device_properties2": (1, 1),
    "VK_KHR_draw_indirect_count": (1, 2),
    "VK_KHR_image_format_list": (1, 2),
    "VK_KHR_maintenance3": (1, 2),
    "VK_KHR_sampler_mirror_clamp_to_edge": (1, 2),
    "VK_EXT_descriptor_indexing": (1, 2),
    "VK_EXT_shader_viewport_index_layer": (1, 2),
    "VK_KHR_timeline_semaphore": (1, 3),
    "VK_KHR_zero_initialize_workgroup_memory": (1, 3),
    "VK_EXT_shader_demote_to_helper_invocation": (1, 3),
    "VK_EXT_texel_buffer_alignment": (1, 3),
}


def merge_dicts(dict1, dict2):
    for k, v in dict2.items():
        if isinstance(v, dict) and k in dict1 and isinstance(dict1[k], dict):
            merge_dicts(dict1[k], v)
        elif isinstance(v, list) and k in dict1 and isinstance(dict1[k], list):
            dict1[k] = list(set(dict1[k] + v))
        else:
            dict1[k] = v


def parse_api_version(val):
    if isinstance(val, str):
        try:
            parts = list(map(int, val.split(".")))
            return tuple(parts + [0] * (3 - len(parts)))
        except ValueError:
            return (1, 0, 0)
    major = val >> 22
    minor = (val >> 12) & 0x3FF
    patch = val & 0xFFF
    return (major, minor, patch)


def evaluate_limit(limit_name, device_val, req_val):
    """Compares device properties/limits with requirements."""
    if device_val is None:
        return False, f"Not specified/supported by device (Requires {req_val})"

    if isinstance(req_val, list):
        if not isinstance(device_val, list):
            return False, f"Device value is not a list structure (Expected {req_val})"
        missing_elems = [x for x in req_val if x not in device_val]
        if missing_elems:
            return False, f"Device missing required flags: {missing_elems}"
        return True, ""

    at_most_limits = {
        "bufferImageGranularity",
        "optimalBufferCopyOffsetAlignment",
        "optimalBufferCopyRowPitchAlignment",
        "nonCoherentAtomSize",
        "minMemoryMapAlignment",
        "minTexelBufferOffsetAlignment",
        "minUniformBufferOffsetAlignment",
        "minStorageBufferOffsetAlignment"
    }

    if limit_name in at_most_limits:
        if device_val <= req_val:
            return True, ""
        return False, f"Device value {device_val} exceeds maximum allowed value of {req_val}"

    if limit_name == "viewportBoundsRange":
        if isinstance(device_val, list) and isinstance(req_val, list) and len(device_val) >= 2 and len(req_val) >= 2:
            ok0 = device_val[0] <= req_val[0]
            ok1 = device_val[1] >= req_val[1]
            if not (ok0 and ok1):
                return False, f"Viewport bounds {device_val} insufficient (Requires bounds extending to {req_val})"
            return True, ""

    if isinstance(req_val, bool):
        if bool(device_val) >= req_val:
            return True, ""
        return False, f"Device value is {device_val} (Expected {req_val})"

    if isinstance(req_val, (int, float)) and isinstance(device_val, (int, float)):
        if device_val >= req_val:
            return True, ""
        return False, f"Device value {device_val} is less than required minimum of {req_val}"

    if device_val == req_val:
        return True, ""
    return False, f"Device value is {device_val} (Expected {req_val})"


class ProfileMatcher:
    def __init__(self, device_profile_data, requirements_doc):
        self.dev_root = device_profile_data.get("capabilities", {}).get("device", {})
        self.req_root = requirements_doc

        properties = self.dev_root.get("properties", {})
        vk_props = properties.get("VkPhysicalDeviceProperties", {})
        vk12_props = properties.get("VkPhysicalDeviceVulkan12Properties", {})
        api_val = vk_props.get("apiVersion", 0)
        self.device_api_version = parse_api_version(api_val)
        self.device_name = vk12_props.get("driverInfo", "Unknown Device")
        self.device_extensions = set(self.dev_root.get("extensions", {}).keys())

    def resolve_target_requirements(self, profile_name):
        profile = self.req_root.get("profiles", {}).get(profile_name, {})
        caps_list = profile.get("capabilities", [])

        req_extensions = {}
        req_features = {}
        req_properties = {}
        req_formats = {}
        req_queue_families = []

        for cap_name in caps_list:
            cap_block = self.req_root.get("capabilities", {}).get(cap_name, {})
            merge_dicts(req_extensions, cap_block.get("extensions", {}))
            merge_dicts(req_features, cap_block.get("features", {}))
            merge_dicts(req_properties, cap_block.get("properties", {}))
            merge_dicts(req_formats, cap_block.get("formats", {}))
            if "queueFamiliesProperties" in cap_block:
                req_queue_families.extend(cap_block["queueFamiliesProperties"])

        return {
            "extensions": req_extensions,
            "features": req_features,
            "properties": req_properties,
            "formats": req_formats,
            "queue_families": req_queue_families,
            "api-version": profile.get("api-version", "1.0.0"),
            "description": profile.get("description", "")
        }

    def evaluate_profile(self, profile_name):
        reqs = self.resolve_target_requirements(profile_name)

        missing_exts = []
        missing_feats = []
        failed_props = []
        failed_formats = []
        failed_queues = []

        for ext, req_ver in sorted(reqs["extensions"].items()):
            supported = (ext in self.device_extensions)
            if not supported and ext in PROMOTED_EXTENSIONS:
                prom_major, prom_minor = PROMOTED_EXTENSIONS[ext]
                if (self.device_api_version[0] > prom_major) or \
                   (self.device_api_version[0] == prom_major and self.device_api_version[1] >= prom_minor):
                    supported = True
            if not supported:
                missing_exts.append(f"{ext} (Required version: {req_ver})")

        dev_feats = self.dev_root.get("features", {})
        for struct_name, req_fields in sorted(reqs["features"].items()):
            for field, req_val in req_fields.items():
                if not req_val:
                    continue

                device_val = None
                has_dev_feat = False
                if struct_name in dev_feats:
                    device_val = dev_feats[struct_name].get(field)
                    has_dev_feat = True

                if device_val is None:
                    device_val = dev_feats.get("VkPhysicalDeviceVulkan14Features", {}).get(field)
                if device_val is None:
                    device_val = dev_feats.get("VkPhysicalDeviceVulkan13Features", {}).get(field)
                if device_val is None:
                    device_val = dev_feats.get("VkPhysicalDeviceVulkan12Features", {}).get(field)
                if device_val is None:
                    device_val = dev_feats.get("VkPhysicalDeviceVulkan11Features", {}).get(field)

                if device_val is None:
                    for s_name, f_dict in dev_feats.items():
                        if field in f_dict:
                            device_val = f_dict[field]
                            break

                if not device_val:
                    if has_dev_feat:
                        has_partial = "VkPhysicalDeviceFeatures" in field or "VkPhysicalDeviceVulkan1" in field
                        missing_feats.append(f"{struct_name} -> {field} {"(partial)" if has_partial else ""}")
                    else:
                        missing_feats.append(f"{struct_name} (missing feature struct) -> {field}")

        dev_props = self.dev_root.get("properties", {})
        for struct_name, req_fields in sorted(reqs["properties"].items()):
            for field, req_val in req_fields.items():
                if isinstance(req_val, dict):
                    device_nested = dev_props.get(struct_name, {}).get(field, {})
                    for nested_key, nested_val in req_val.items():
                        dev_val = device_nested.get(nested_key)
                        passed, err_msg = evaluate_limit(nested_key, dev_val, nested_val)
                        if not passed:
                            failed_props.append(f"{struct_name} -> {field} -> {nested_key}: {err_msg}")
                else:
                    dev_val = dev_props.get(struct_name, {}).get(field)
                    if dev_val is None:
                        dev_val = dev_props.get("VkPhysicalDeviceVulkan14Properties", {}).get(field)
                    if dev_val is None:
                        dev_val = dev_props.get("VkPhysicalDeviceVulkan13Properties", {}).get(field)
                    if dev_val is None:
                        dev_val = dev_props.get("VkPhysicalDeviceVulkan12Properties", {}).get(field)
                    if dev_val is None:
                        dev_val = dev_props.get("VkPhysicalDeviceVulkan11Properties", {}).get(field)
                    passed, err_msg = evaluate_limit(field, dev_val, req_val)
                    if not passed:
                        failed_props.append(f"{struct_name} -> {field}: {err_msg}")

        dev_formats = self.dev_root.get("formats", {})
        for fmt_name, req_fmt_data in sorted(reqs["formats"].items()):
            if fmt_name not in dev_formats:
                failed_formats.append(f"{fmt_name}: Format entirely unsupported on target device")
                continue

            dev_fmt_props = dev_formats[fmt_name].get("VkFormatProperties", {})
            req_fmt_props = req_fmt_data.get("VkFormatProperties", {})

            for feature_type, req_features_list in req_fmt_props.items():
                dev_features_list = dev_fmt_props.get(feature_type, [])
                missing_flags = [flag for flag in req_features_list if flag not in dev_features_list]
                if missing_flags:
                    failed_formats.append(f"{fmt_name} -> {feature_type} missing flags: {', '.join(missing_flags)}")

        dev_queues = self.dev_root.get("queueFamiliesProperties", [])
        for req_q in reqs["queue_families"]:
            req_q_props = req_q.get("VkQueueFamilyProperties", {})
            req_flags = set(req_q_props.get("queueFlags", []))
            req_count = req_q_props.get("queueCount", 1)

            matched = False
            for dev_q in dev_queues:
                dev_q_props = dev_q.get("VkQueueFamilyProperties", {})
                dev_flags = set(dev_q_props.get("queueFlags", []))
                dev_count = dev_q_props.get("queueCount", 0)

                if req_flags.issubset(dev_flags) and dev_count >= req_count:
                    matched = True
                    break

            if not matched:
                failed_queues.append(f"Requires queue family with flags: {list(req_flags)} (Minimum Count: {req_count})")

        all_passed = not (missing_exts or missing_feats or failed_props or failed_formats or failed_queues)

        return {
            "passed": all_passed,
            "description": reqs["description"],
            "api-version": reqs["api-version"],
            "missing_extensions": missing_exts,
            "missing_features": missing_feats,
            "failed_properties": failed_props,
            "failed_formats": failed_formats,
            "failed_queues": failed_queues
        }


def main():
    if len(sys.argv) < 2:
        print("Usage: python vkd3d_profile_matcher.py <hardware_profile.json>")
        sys.exit(1)

    hw_path = sys.argv[1]
    req_path = "VP_D3D12_VKD3D_PROTON_profile.json"

    for path in [hw_path, req_path]:
        if not os.path.exists(path):
            print(f"Error: File '{path}' not found.")
            sys.exit(1)

    try:
        with open(hw_path, "r", encoding="utf-8") as f:
            hw_data = json.load(f)
    except json.JSONDecodeError as err:
        print(f"Error parsing Hardware Profile JSON: {err}")
        sys.exit(1)

    try:
        with open(req_path, "r", encoding="utf-8") as f:
            req_data = json.load(f)
    except json.JSONDecodeError as err:
        print(f"Error parsing Requirements Profile JSON: {err}")
        sys.exit(1)

    matcher = ProfileMatcher(hw_data, req_data)

    print(f"Driver: {matcher.device_name}")
    print(f"Vulkan API: {'.'.join(map(str, matcher.device_api_version))}")
    print()

    profiles = req_data.get("profiles", {})
    for profile_name in sorted(profiles.keys()):
        result = matcher.evaluate_profile(profile_name)
        status = "PASSED" if result["passed"] else "FAILED"

        print(f"[{status}] Profile: {profile_name}")
        print(f"  Description: {result['description']}")
        print(f"  Target Vulkan API: {result['api-version']}")

        if not result["passed"]:
            if result["missing_extensions"]:
                print("  - Missing/Insufficient Extensions:")
                for ext in result["missing_extensions"]:
                    print(f"      * {ext}")
            if result["missing_features"]:
                print("  - Missing Required Feature Flags:")
                for feat in result["missing_features"]:
                    print(f"      * {feat}")
            if result["failed_properties"]:
                print("  - Failed Properties or Limits Constraints:")
                for prop in result["failed_properties"]:
                    print(f"      * {prop}")
            if result["failed_formats"]:
                print("  - Missing Format Buffer/Tiling Features:")
                for fmt in result["failed_formats"]:
                    print(f"      * {fmt}")
            if result["failed_queues"]:
                print("  - Failed Queue Family Constraints:")
                for q in result["failed_queues"]:
                    print(f"      * {q}")
        print("-" * 65 + "\n")


if __name__ == "__main__":
    main()
