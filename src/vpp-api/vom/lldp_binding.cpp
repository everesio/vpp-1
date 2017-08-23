/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <cassert>
#include <iostream>

#include "vom/lldp_binding.hpp"
#include "vom/cmd.hpp"

using namespace VPP;

/**
 * A DB of all LLDP configs
 */
singular_db<interface::key_type, lldp_binding> lldp_binding::m_db;

lldp_binding::event_handler lldp_binding::m_evh;

lldp_binding::lldp_binding(const interface &itf,
                           const std::string &port_desc):
    m_itf(itf.singular()),
    m_port_desc(port_desc),
    m_binding(0)
{
}

lldp_binding::lldp_binding(const lldp_binding& o):
    m_itf(o.m_itf),
    m_port_desc(o.m_port_desc),
    m_binding(0)
{
}

lldp_binding::~lldp_binding()
{
    sweep();

    // not in the DB anymore.
    m_db.release(m_itf->key(), this);
}

void lldp_binding::sweep()
{
    if (m_binding)
    {
        HW::enqueue(new unbind_cmd(m_binding, m_itf->handle()));
    }
    HW::write();
}

void lldp_binding::dump(std::ostream &os)
{
    m_db.dump(os);
}

void lldp_binding::replay()
{
    if (m_binding)
    {
        HW::enqueue(new bind_cmd(m_binding, m_itf->handle(), m_port_desc));
    }
}

std::string lldp_binding::to_string() const
{
    std::ostringstream s;
    s << "Lldp-binding: " << m_itf->to_string()
      << " port_desc:" << m_port_desc
      << " " << m_binding.to_string();

    return (s.str());
}

void lldp_binding::update(const lldp_binding &desired)
{
    /*
     * the desired state is always that the interface should be created
     */
    if (!m_binding)
    {
        HW::enqueue(new bind_cmd(m_binding, m_itf->handle(), m_port_desc));
    }
}

std::shared_ptr<lldp_binding> lldp_binding::find_or_add(const lldp_binding &temp)
{
    return (m_db.find_or_add(temp.m_itf->key(), temp));
}

std::shared_ptr<lldp_binding> lldp_binding::singular() const
{
    return find_or_add(*this);
}

lldp_binding::event_handler::event_handler()
{
    OM::register_listener(this);
    inspect::register_handler({"lldp"}, "LLDP bindings", this);
}

void lldp_binding::event_handler::handle_replay()
{
    m_db.replay();
}

void lldp_binding::event_handler::handle_populate(const client_db::key_t &key)
{
    // FIXME
}

dependency_t lldp_binding::event_handler::order() const
{
    return (dependency_t::BINDING);
}

void lldp_binding::event_handler::show(std::ostream &os)
{
    m_db.dump(os);
}
