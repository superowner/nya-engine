//https://code.google.com/p/nya-engine/

#pragma once

// ToDo: working lru

#include "resources.h"
#include "memory/pool.h"
#include <map>
#include <string>

namespace nya_resources
{

template<typename t_res,int block_count> class shared_resources
{

typedef shared_resources<t_res,block_count> t_creator;

struct res_holder;

public:
    class shared_resource_ref
    {
        template<typename,int> friend class shared_resources;

    public:
        bool is_valid() const { return m_res!=0; }
        const t_res *const_get() const { return m_res; }
        const t_res *operator -> () const { return m_res; };

        const char *get_name() const
        {
            if(!m_creator)
                return 0;

            return m_creator->get_res_name(*this);
        }

        void free()
        {
            if(m_creator)
                m_creator->free(*this);

            m_res=0;
            m_res_holder=0;
            m_creator=0;
        }

    public:
        shared_resource_ref(): m_res(0), m_res_holder(0), m_creator(0) {}

        shared_resource_ref(const shared_resource_ref &ref)
        {
            m_res=ref.m_res;
            m_res_holder=ref.m_res_holder;
            m_creator=ref.m_creator;

            ref_count_inc();
        }

        shared_resource_ref &operator=(const shared_resource_ref &ref) 
        {
			if(this==&ref)
				return *this;

			free();

            m_res=ref.m_res;
            m_res_holder=ref.m_res_holder;
            m_creator=ref.m_creator;

            ref_count_inc();

            return *this;
        }

        ~shared_resource_ref() { free(); }

    protected:
        shared_resource_ref(t_res*res,res_holder*holder,t_creator *creator):
        m_res(res),m_res_holder(holder),m_creator(creator) {}

    private:
        void ref_count_inc()
        {
            if(m_creator)
                m_creator->res_ref_count_inc(*this);
        }

    protected:
        t_res *m_res;

    private:
        res_holder *m_res_holder;
        t_creator *m_creator;
    };

    class shared_resource_mutable_ref: public shared_resource_ref
    {
        template<typename,int> friend class shared_resources;

    public:
        t_res *get() { return this->m_res; }
        t_res *operator -> () { return this->m_res; }

    public:
        shared_resource_mutable_ref() { shared_resource_ref(); }

    private:
        shared_resource_mutable_ref(t_res*res,res_holder*holder,t_creator *creator)
        { *(shared_resource_ref*)this=shared_resource_ref(res,holder,creator); }
    };

public:
    shared_resource_ref access(const char*name)
    {
        std::pair<resources_map_iterator,bool> ir = m_res_map.insert(std::make_pair(std::string(name),(res_holder*)0));
        if(ir.second)
        {
            res_holder *holder=m_res_pool.allocate();
            if(!holder)
                return shared_resource_ref();

            ir.first->second = holder;
            if(!fill_resource(name,holder->res))
            {
                m_res_map.erase(ir.first);
                return shared_resource_ref();
            }
            holder->ref_count=1;
            holder->map_it=ir.first;

            holder->lru_next=m_lru_first;
            if(m_lru_first)
                m_lru_first->lru_prev=holder;

            m_lru_first=holder;
            holder->lru_prev=0;

            if(!m_lru_last)
            {
                m_lru_last=holder;
                holder->lru_next=0;
            }

            ++m_used_count;
            free_lru();

            return shared_resource_ref(&(holder->res),holder,this);
        }
        else
        {
            res_holder *holder=ir.first->second;
            if (holder)
            {
                ++holder->ref_count;

                if(holder->lru_prev) //not first
                {
                    holder->lru_prev->lru_next=holder->lru_next;

                    if(holder->lru_next)
                        holder->lru_next->lru_prev=holder->lru_prev;
                    else
                    {
                        m_lru_last=holder->lru_prev;
                        m_lru_last->lru_next=0;
                    }

                    if(m_lru_first)
                        m_lru_first->lru_prev=holder;

                    m_lru_first=holder;
                    m_lru_first->lru_prev=0;
                }

                return shared_resource_ref(&(holder->res),holder,this);
            }
        }

        return shared_resource_ref();
    }

    shared_resource_mutable_ref create()
    {
        res_holder *holder=m_res_pool.allocate();
        if(!holder)
            return shared_resource_mutable_ref();

        holder->ref_count=1;
        holder->map_it=m_res_map.end();

        return shared_resource_mutable_ref(&(holder->res),holder,this);
    }

    void reload_resources()
    {
        for(resources_map_iterator it=m_res_map.begin();
            it!=m_res_map.end();++it)
        {
            if(!it->second || it->first.empty())
                continue;

            release_resource(it->second->res);
            fill_resource(it->first.c_str(),it->second->res);
        }
    }

    void reload_resource(const char *name)
    {
        if(!name)
            return;

        resources_map_iterator it=m_res_map.find(name);
        if(it==m_res_map.end() || !it->second)
            return;

        release_resource(it->second->res);
        fill_resource(it->first.c_str(),it->second->res);
    }

    const char *get_res_name(const shared_resource_ref&ref)
    {
        if(!ref.m_res_holder)
            return 0;

        if(ref.m_res_holder->map_it==m_res_map.end())
            return 0;

        return ref.m_res_holder->map_it->first.c_str();
    }

    void free(shared_resource_ref&ref)
    {
        if(!ref.m_res_holder)
            return;

        --ref.m_res_holder->ref_count;

        if(ref.m_res_holder->ref_count>0)
            return;

		ref.m_res_holder->ref_count=0;

        if(!m_should_unload_unused)
            return;

        if(ref.m_res)
            release_resource(*ref.m_res);

        if(ref.m_res_holder->map_it!=m_res_map.end())
            m_res_map.erase(ref.m_res_holder->map_it);

        m_res_pool.free(ref.m_res_holder);

        //ToDo: unlink from lru list

        --m_used_count;
    }

    void res_ref_count_inc(shared_resource_ref&ref)
    {
        if(!ref.m_res_holder)
            return;

        ++ref.m_res_holder->ref_count;
    }

    void should_unload_unused(bool unload)
    {
        if(unload && unload!=m_should_unload_unused)
            free_unused();

        m_should_unload_unused=unload;
    }

    void free_unused()
    {
        resources_map_iterator it=m_res_map.begin();
        while(it!=m_res_map.end())
        {
            if(it->second)
            {
                if(it->second->ref_count>0)
                {
                    ++it;
                    continue;
                }
/*
                if(it->second->lru_next)
                    it->second->lru_next->lru_prev=it->second->lru_prev;
                else
                    m_lru_last=it->second->lru_prev;

                if(it->second->lru_prev)
                    it->second->lru_prev->lru_next=it->second->lru_next;
                else
                    m_lru_first=it->second->lru_next;
*/
                --m_used_count;

                release_resource(it->second->res);
                m_res_pool.free(it->second);
            }

            resources_map_iterator er = it;
            ++it;

            m_res_map.erase(er);
        }
    }

    void free_all()
    {
        resources_map_iterator it;
        for(it=m_res_map.begin();it!=m_res_map.end();++it)
        {
            if(it->second)
                release_resource(it->second->res);
        }

        m_res_map.clear();
        m_res_pool.clear();

        m_lru_first=m_lru_last=0;
        m_used_count=0;
    }
    /* //ToDo
      // note: lru removes only unused resources
    void set_lru_limit(size_t limit)
    {
        m_lru_limit=limit;
        free_lru();
    }
     */
private:
    void free_lru()
    {
        //get_log()<<(unsigned int)m_used_count<<" ";

        //if(!m_lru_limit)
            return;

        res_holder *last=m_lru_last;

    int test=0;
        while(last && m_used_count>m_lru_limit)
        {
            res_holder *prev=last->lru_prev;

            if(!last->ref_count)
            {
                if(last->lru_next)
                    last->lru_next->lru_prev=last->lru_prev;
                else
                {
                    m_lru_last=last->lru_prev;
                    if(m_lru_last)
                        m_lru_last->lru_next=0;
                }

                if(last->lru_prev)
                    last->lru_prev->lru_next=last->lru_next;
                else
                {
                    m_lru_first=last->lru_next;
                    if(m_lru_first)
                        m_lru_first->lru_prev=0;
                }

                release_resource(last->res);

                m_res_map.erase(last->map_it);

                m_res_pool.free(last);

                --m_used_count;
            }

            last=prev;
            ++test;
        }

        get_log()<<test<<"\n";

    }

public:
    ~shared_resources()
    {
        unsigned int unreleased_count=0;
        resources_map_iterator it;
        for(it=m_res_map.begin();it!=m_res_map.end();++it)
        {
            if(it->second)
            {
                get_log()<<"warning: unreleased resource: "<<it->first.c_str()<<"\n";
                ++unreleased_count;
            }
        }

        if(unreleased_count)
            get_log()<<"warning: unreleased resources count: "<<unreleased_count<<"\n";

        m_res_map.clear();
        m_res_pool.clear();
    }

private:
    virtual bool fill_resource(const char *name,t_res &res) { return false; }
    virtual bool release_resource(t_res &res) { return false; }
    //virtual t_res *get_resource(const res_holder *holder) { return &holder->res; }

public:
    shared_resources(): m_lru_first(0),m_lru_last(0),m_used_count(0),m_lru_limit(0)
                        ,m_should_unload_unused(true) {}

    //non copiable
private:
    shared_resources(const shared_resources &);
    void operator = (const shared_resources &);

private:
    typedef std::map<std::string,res_holder*> resources_map;
    typedef typename resources_map::iterator resources_map_iterator;

private:
    struct res_holder
    {
        t_res res;
        int ref_count;
        resources_map_iterator map_it;

        res_holder *lru_prev;
        res_holder *lru_next;

        res_holder(): ref_count(0), lru_prev(0), lru_next(0) {}
    };

    res_holder *m_lru_first;
    res_holder *m_lru_last;
    size_t m_used_count;
    size_t m_lru_limit;

    resources_map m_res_map;
    nya_memory::pool<res_holder,block_count> m_res_pool;

private:
    bool m_should_unload_unused;
};

}
