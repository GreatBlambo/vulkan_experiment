import json
import argparse
import os

def flatten_type(type_name, types):
    if type_name not in types:
        return type_name
    result = types[type_name]
    new_members = []
    for member in result["members"]:
        f_type = flatten_type(member["type"], types)
        if isinstance(f_type, str):
            new_members.append(member)
        else:
            for f_member in f_type["members"]:
                f_member["offset"] += member["offset"]
            new_members.extend(f_type["members"])

    result["members"] = new_members
    return result

def extract_descriptors(config):
    textures = config.get("textures", [])
    ubos = config.get("ubos", [])
    types = config.get("types", {})
    
    result = []
    for texture in textures:
        texture["type"] = flatten_type(texture["type"], types)
        result.append(texture)
    
    for ubo in ubos:
        ubo["type"] = flatten_type(ubo["type"], types)
        result.append(ubo)

    return result

def insert_descriptors(descriptor_sets, descriptors, shaders_obj):
    for descriptor in descriptors:
        d_set = descriptor["set"]
        d_binding = descriptor["binding"]
        # Initialize descriptor set if it doesn't exist
        if d_set not in descriptor_sets:
            descriptor_sets[d_set] = {}
        # Insert descriptor if it doesn't exist
        if d_binding not in descriptor_sets[d_set]:
            descriptor_sets[d_set][d_binding] = descriptor
        # Assert that descriptor is same if it does exist
        elif json.dumps(descriptor_sets[d_set][d_binding]) != json.dumps(descriptor):
            raise ValueError(f"Descriptor conflict when attempting to link {shaders_obj} at set = {d_set}, binding = {d_binding}.")

def main():
    parser = argparse.ArgumentParser(description="Generate a list of vulkan resources to create based on a renderer config")
    parser.add_argument("render_config", type=str)
    parser.add_argument("--out_dir", dest="out_dir", type=str, default="./")
    args = parser.parse_args()

    with open(args.render_config) as f:
        config = json.loads(f.read())
    
    pipeline_layouts = []
    pipelines = []
    render_passes = []
    for render_pass in config["passes"]:
        shaders_obj = render_pass["shaders"]
        if "comp" in shaders_obj:
            # Compute pass
            if len(shaders_obj.keys()) > 1:
                raise ValueError("Compute shaders can't go with any other shader")
            raise NotImplementedError("Compute passes not yet implemented")
        else:
            # Graphics pass
            # Extract/merge descriptor sets, extract vertex inputs
            descriptor_sets = {}
            vertex_inputs = {}
            for stage, shader_config in shaders_obj.items():
                if isinstance(shader_config, str):
                    with open(shader_config) as f:
                        config_dict = json.loads(f.read())
                    shader_config = config_dict
                descriptors = extract_descriptors(shader_config)
                insert_descriptors(descriptor_sets, descriptors, shaders_obj)

                if stage == "vert":
                    for inp in shader_config["inputs"]:
                        vertex_inputs[inp["name"]] = inp
            print(f"Descriptor sets: {descriptor_sets}")
            print(f"Vertex inputs: {vertex_inputs}")

            # TODO: Match inputs and outputs between stages
            # TODO: Match input layouts to inputs
        # TODO: Attempt to merge pipeline layouts and pipelines
        # TODO: Create render pass
    # TODO: Do fancy render graph stuff

if __name__ == "__main__":
    main()