/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <iostream>
#include <algorithm>

#include "vom/dhcp_config.hpp"

DEFINE_VAPI_MSG_IDS_DHCP_API_JSON;

using namespace VPP;

dhcp_config::bind_cmd::bind_cmd(HW::item<bool> &item,
                                const handle_t &itf,
                                const std::string &hostname,
                                const l2_address_t &client_id):
    rpc_cmd(item),
    m_itf(itf),
    m_hostname(hostname),
    m_client_id(client_id)
{
}

bool dhcp_config::bind_cmd::operator==(const bind_cmd& other) const
{
    return ((m_itf == other.m_itf) &&
            (m_hostname == other.m_hostname));
}

rc_t dhcp_config::bind_cmd::issue(connection &con)
{
    msg_t req(con.ctx(), std::ref(*this));

    auto &payload = req.get_request().get_payload();
    payload.sw_if_index = m_itf.value();
    payload.is_add = 1;
    payload.pid = getpid();
    payload.want_dhcp_event = 1;
    
    memcpy(payload.hostname,
           m_hostname.c_str(),
           std::min(sizeof(payload.hostname),
                    m_hostname.length()));

    memset(payload.client_id, 0, sizeof(payload.client_id));
    payload.client_id[0] = 1;
    std::copy_n(begin(m_client_id.bytes),
                std::min(sizeof(payload.client_id),
                         m_client_id.bytes.size()),
                payload.client_id+1);

    VAPI_CALL(req.execute());

    m_hw_item.set(wait());

    return rc_t::OK;
}

std::string dhcp_config::bind_cmd::to_string() const
{
    std::ostringstream s;
    s << "Dhcp-config-bind: " << m_hw_item.to_string()
      << " itf:" << m_itf.to_string()
      << " hostname:" << m_hostname;

    return (s.str());
}

dhcp_config::unbind_cmd::unbind_cmd(HW::item<bool> &item,
                                    const handle_t &itf,
                                    const std::string &hostname):
    rpc_cmd(item),
    m_itf(itf),
    m_hostname(hostname)
{
}

bool dhcp_config::unbind_cmd::operator==(const unbind_cmd& other) const
{
    return ((m_itf == other.m_itf) &&
            (m_hostname == other.m_hostname));
}

rc_t dhcp_config::unbind_cmd::issue(connection &con)
{
    msg_t req(con.ctx(), std::ref(*this));

    auto &payload = req.get_request().get_payload();
    payload.sw_if_index = m_itf.value();
    payload.is_add = 0;
    payload.pid = getpid();
    payload.want_dhcp_event = 0;
    
    memcpy(payload.hostname,
           m_hostname.c_str(),
           std::min(sizeof(payload.hostname),
                    m_hostname.length()));
    
    VAPI_CALL(req.execute());

    wait();
    m_hw_item.set(rc_t::NOOP);

    return rc_t::OK;
}

std::string dhcp_config::unbind_cmd::to_string() const
{
    std::ostringstream s;
    s << "Dhcp-config-unbind: " << m_hw_item.to_string()
      << " itf:" << m_itf.to_string()
      << " hostname:" << m_hostname;

    return (s.str());
}

dhcp_config::events_cmd::events_cmd(event_listener &el):
    event_cmd(),
    m_listener(el)
{
}

bool dhcp_config::events_cmd::operator==(const events_cmd& other) const
{
    return (true);
}

rc_t dhcp_config::events_cmd::issue(connection &con)
{
    /*
     * Set the call back to handle DHCP complete envets.
     */
    m_reg.reset(new reg_t(con.ctx(), std::ref(*this)));

    /*
     * return in-progress so the command stays in the pending list.
     */
    return (rc_t::INPROGRESS);
}

void dhcp_config::events_cmd::retire()
{
}

void dhcp_config::events_cmd::notify()
{
    m_listener.handle_dhcp_event(this);
}

std::string dhcp_config::events_cmd::to_string() const
{
    return ("dhcp-events");
}
