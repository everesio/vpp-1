/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef __VPP_LLDP_GLOBAL_H__
#define __VPP_LLDP_GLOBAL_H__

#include <string>
#include <map>
#include <vector>
#include <stdint.h>

#include "vom/object_base.hpp"
#include "vom/om.hpp"
#include "vom/hw.hpp"
#include "vom/rpc_cmd.hpp"
#include "vom/dump_cmd.hpp"
#include "vom/singular_db.hpp"
#include "vom/interface.hpp"
#include "vom/sub_interface.hpp"
#include "vom/inspect.hpp"

#include <vapi/lldp.api.vapi.hpp>

namespace VPP
{
    /**
     * A representation of LLDP global configuration
     */
    class lldp_global: public object_base
    {
    public:
        /**
         * Construct a new object matching the desried state
         */
        lldp_global(const std::string &system_name,
                    uint32_t tx_hold,
                    uint32_t tx_interval);

        /**
         * Copy Constructor
         */
        lldp_global(const lldp_global& o);

        /**
         * Destructor
         */
        ~lldp_global();

        /**
         * Return the 'singular' of the LLDP global that matches this object
         */
        std::shared_ptr<lldp_global> singular() const;

        /**
         * convert to string format for debug purposes
         */
        std::string to_string() const;

        /**
         * Dump all LLDP globals into the stream provided
         */
        static void dump(std::ostream &os);

        /**
         * A command class that binds the LLDP global to the interface
         */
        class config_cmd: public rpc_cmd<HW::item<bool>, rc_t, vapi::Lldp_config>
        {
        public:
            /**
             * Constructor
             */
            config_cmd(HW::item<bool> &item,
                       const std::string &system_name,
                       uint32_t tx_hold,
                       uint32_t tx_interval);

            /**
             * Issue the command to VPP/HW
             */
            rc_t issue(connection &con);
            /**
             * convert to string format for debug purposes
             */
            std::string to_string() const;

            /**
             * Comparison operator - only used for UT
             */
            bool operator==(const config_cmd&i) const;
        private:
            /**
             * The system name
             */
            const std::string m_system_name;

            /**
             * TX timer configs
             */
            uint32_t m_tx_hold;
            uint32_t m_tx_interval;
        };

    private:
        /**
         * Class definition for listeners to OM events
         */
        class event_handler: public OM::listener, public inspect::command_handler
        {
        public:
            event_handler();
            virtual ~event_handler() = default;

            /**
             * Handle a populate event
             */
            void handle_populate(const client_db::key_t & key);

            /**
             * Handle a replay event
             */
            void handle_replay();

            /**
             * Show the object in the Singular DB
             */
            void show(std::ostream &os);

            /**
             * Get the sortable Id of the listener
             */
            dependency_t order() const;
        };

        /**
         * event_handler to register with OM
         */
        static event_handler m_evh;

        /**
         * Enquue commonds to the VPP command Q for the update
         */
        void update(const lldp_global &obj);

        /**
         * Find or add LLDP global to the OM
         */
        static std::shared_ptr<lldp_global> find_or_add(const lldp_global &temp);

        /*
         * It's the VPP::OM class that calls singular()
         */
        friend class VPP::OM;

        /**
         * It's the VPP::singular_db class that calls replay()
         */
        friend class VPP::singular_db<interface::key_type, lldp_global>;

        /**
         * Sweep/reap the object if still stale
         */
        void sweep(void);

        /**
         * replay the object to create it in hardware
         */
        void replay(void);

        /**
         * The system name
         */
        const std::string m_system_name;

        /**
         * TX timer configs
         */
        uint32_t m_tx_hold;
        uint32_t m_tx_interval;

        /**
         * HW globaluration for the binding. The bool representing the
         * do/don't bind.
         */
        HW::item<bool> m_binding;

        /**
         * A map of all Lldp globals keyed against the system name.
         *  there needs to be some sort of key, that will do.
         */
        static singular_db<std::string, lldp_global> m_db;
    };
};

#endif
