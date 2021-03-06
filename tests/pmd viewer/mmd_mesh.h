//https://code.google.com/p/nya-engine/

#pragma once

#include "scene/mesh.h"

#include "load_pmd.h"
#include "load_pmx.h"

class mmd_mesh
{
public:
    bool load(const char *name);
    void unload();

    const char *get_name() const { return m_mesh.get_name(); }

    void set_anim(const nya_scene::animation &anim,int layer=0);
    const nya_scene::animation_proxy & get_anim(int layer=0) const { return m_mesh.get_anim(layer); }
    unsigned int get_anim_time(int layer=0) const { return m_mesh.get_anim_time(layer); }
    bool is_anim_finished(int layer=0) const { return m_mesh.is_anim_finished(layer); }
    void set_anim_time(unsigned int time,int layer=0) { m_mesh.set_anim_time(time,layer); }

    void set_morph(int idx,float value,bool overriden)
    {
        if(idx<0 || idx>=int(m_morphs.size()))
            return;

        m_morphs[idx].value=value;
        m_morphs[idx].overriden=overriden;
    }

    void update(unsigned int dt);
    void draw(const char *pass_name=nya_scene::material::default_pass);
    void draw_group(int group_idx,const char *pass_name=nya_scene::material::default_pass) const;

    int get_groups_count() const { return m_mesh.get_groups_count(); }
    const char *get_group_name(int group_idx) const { return m_mesh.get_group_name(group_idx); }
    const nya_scene::material &get_material(int group_idx) const { return m_mesh.get_material(group_idx); }
    nya_scene::material &modify_material(int group_idx) { return m_mesh.modify_material(group_idx); }
    bool set_material(int group_idx,const nya_scene::material &mat) { return m_mesh.set_material(group_idx,mat); }

    void set_pos(const nya_math::vec3 &pos) { m_mesh.set_pos(pos); }
    void set_rot(float yaw,float pitch,float roll) { m_mesh.set_rot(yaw,pitch,roll); }
    void set_rot(const nya_math::quat &rot) { m_mesh.set_rot(rot); }
    void set_scale(float scale) { m_mesh.set_scale(scale); }

    const nya_render::skeleton &get_skeleton() const { return internal().get_skeleton(); }

    int get_morphs_count() const { return (int)m_morphs.size(); }
    const char *get_morph_name(int idx) const
    {
        if(!m_morph_data || idx<0 || idx>=get_morphs_count())
            return 0;

        return m_morph_data->morphs[idx].name.c_str();
    }

    pmd_morph_data::morph_type get_morph_type(int idx) const
    {
        if(!m_morph_data || idx<0 || idx>=get_morphs_count())
            return m_morph_data->morph_base;

        return m_morph_data->morphs[idx].type;
    }

    mmd_mesh(): m_morph_data(0), m_is_pmx(false) {}

public:
    const nya_scene::mesh_internal &internal() const { return m_mesh.internal(); }

private:
    nya_scene::mesh m_mesh;
    nya_render::vbo m_vbo;
    std::vector<float> m_vertex_data;
    std::vector<float> m_original_vertex_data;
    const pmd_morph_data *m_morph_data;

    struct morph
    {
        bool overriden;
        float value;
        float last_value;

        morph(): overriden(false), value(0.0f), last_value(0.0f) {}
    };

    std::vector<morph> m_morphs;

    struct applied_anim { int layer; std::vector<int> curves_map; };
    std::vector<applied_anim> m_anims;
    bool m_is_pmx;
};
