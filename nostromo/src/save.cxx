/*
    Nostromo_n50 configuration tools to support Belkin's Nostromo n50
    Copyright (C) 2003 Paul Bohme and others

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "nost_data.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>

static const xmlChar* nstr(int n)
{
  static xmlChar tmp[256];
  sprintf((char*)tmp, "%d", n);
  return tmp;
}

/**
 * Use libxml2 to pump out an XML representation of the config data.
 **/
void save_configs(const char* fname, const nost_data* data)
{
    xmlDoc* doc = NULL;       /* document pointer */

    xmlNode* e_root = NULL;
    xmlNode* e_tmp = NULL;
    xmlNode* e_config = NULL;
    xmlNode* e_mode = NULL;
    xmlNode* e_key = NULL;
    xmlNode* e_stroke = NULL;
    int c, m, k, s;

    doc = xmlNewDoc(BAD_CAST "1.0");
    e_root = xmlNewNode(NULL, BAD_CAST "nostromo");
    xmlDocSetRootElement(doc, e_root);

    e_tmp = xmlNewChild(e_root, NULL, BAD_CAST "current_config", NULL);
    xmlNewProp(e_tmp, BAD_CAST "value", nstr(data->current_config));

    e_tmp = xmlNewChild(e_root, NULL, BAD_CAST "networking", NULL);
    xmlNewProp(e_tmp, BAD_CAST "enabled", nstr(data->network_enabled));
    xmlNewProp(e_tmp, BAD_CAST "port", nstr(data->port));
    xmlNewProp(e_tmp, BAD_CAST "server", BAD_CAST data->server);

    for(c = 0; c < data->num_configs; c++) {
        e_config = xmlNewChild(e_root, NULL, BAD_CAST "config", NULL);
        xmlNewProp(e_config, BAD_CAST "name", BAD_CAST data->configs[c].name);
        xmlNewProp(e_config, BAD_CAST "model", nstr((int)data->configs[c].model));
        for(m = 0; m < MAX_MODES; m++) {
            e_mode = xmlNewChild(e_config, NULL, BAD_CAST "mode", NULL);
            xmlNewProp(e_mode, BAD_CAST "num", nstr(m));
            for(k = 0; k < MAX_KEYS; k++) {
                e_key = xmlNewChild(e_mode, NULL, BAD_CAST "key", NULL);
                xmlNewProp(e_key, BAD_CAST "name", BAD_CAST data->configs[c].keys[m][k].name);
                xmlNewProp(e_key, BAD_CAST "type", nstr(data->configs[c].keys[m][k].type));
                xmlNewProp(e_key, BAD_CAST "repeat", nstr(data->configs[c].keys[m][k].repeat));
                xmlNewProp(e_key, BAD_CAST "delay", nstr(data->configs[c].keys[m][k].repeat_delay));
                xmlNewProp(e_key, BAD_CAST "num", nstr(k));
                xmlNewProp(e_key, BAD_CAST "remote", nstr(data->configs[c].keys[m][k].remote));
                for(s = 0; s < data->configs[c].keys[m][k].key_count; s++) {
                    e_stroke = xmlNewChild(e_key, NULL, BAD_CAST "stroke", NULL);
                    xmlNewProp(e_stroke, BAD_CAST "type", nstr((int)data->configs[c].keys[m][k].data[s].type));
                    xmlNewProp(e_stroke, BAD_CAST "code", nstr(data->configs[c].keys[m][k].data[s].code));
                    xmlNewProp(e_stroke, BAD_CAST "state", nstr(data->configs[c].keys[m][k].data[s].state));
                    xmlNewProp(e_stroke, BAD_CAST "display", BAD_CAST data->configs[c].keys[m][k].data[s].display);
                    xmlNewProp(e_stroke, BAD_CAST "delay", nstr(data->configs[c].keys[m][k].data[s].delay));
                }
            }
        }
    }
    xmlSaveFile(fname, doc);
    xmlFreeDoc(doc);
}

