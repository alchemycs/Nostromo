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

#include <string.h>
#include <stdio.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

/**
 * Find a node by name, sucks that I can't find this in libxml2?
 **/
xmlNode* find_node(xmlNode* parent, const char* name) {
    xmlNode* found = NULL;
    for(found = parent->children; found; found = found->next) {
        if(!strcmp(name, (char*)found->name)) {
            break;
        }
    }

    return found;
}

/**
 * Pull an int value from an attribute, if it's there.
 * If not, don't touch the value.
 **/
void get_int_attr(xmlNode* node, const char* name, int* val) {
    xmlChar* attr = xmlGetProp(node, BAD_CAST name);
    if(attr) {
        *val = atoi((char*)attr);
        xmlFree(attr);
    }
}

/**
 * Pull a string value from an attribute, if it's there.
 * If not, don't touch the value.
 * Caller is responsible for freeing memory.
 **/
void get_str_attr(xmlNode* node, const char* name, char** val) {
    xmlChar* attr = xmlGetProp(node, BAD_CAST name);
    if(attr) {
        *val = strdup((char*)attr);
        xmlFree(attr);
    }
}

/**
 *
 **/
nost_data* load_configs(const char* fname)
{
    nost_data* data;
    xmlDoc* tree = NULL;
    xmlNode* e_root = NULL;
    xmlNode* e_tmp = NULL;
    xmlNode** e_config_list = NULL;
    xmlNode* e_config = NULL;
    xmlNode* e_mode = NULL;
    xmlNode* e_key = NULL;
    xmlNode* e_stroke = NULL;
    int c, m, k, s;

    data = (nost_data*)calloc(1, sizeof(nost_data));
    if(data == NULL) {
        return NULL;
    }
    data->current_config = -1;
    data->server = strdup("");

    if((tree = xmlParseFile(fname)) == NULL) {
        fprintf(stderr, "Failed to load [%s]\n", fname);
        return data;
    }

    if((e_root = xmlDocGetRootElement(tree)) == NULL) {
        return data;
    }

    /* Pull a current config value from the file */
    if((e_tmp = find_node(e_root, "current_config")) != NULL) {
        get_int_attr(e_tmp, "value", &data->current_config);
    } else {
        printf("Current config not found?\n");
    }

    /* Pull a network config values from the file */
    if((e_tmp = find_node(e_root, "networking")) != NULL) {
        get_int_attr(e_tmp, "enabled", &data->network_enabled);
        get_int_attr(e_tmp, "port", &data->port);
        get_str_attr(e_tmp, "server", &data->server);
    }

    for(data->num_configs = 0, e_tmp = e_root->children; e_tmp = e_tmp->next; e_tmp) {
        if(!strcmp((char*)e_tmp->name, "config")) {
            data->num_configs++;
        }
    }

    if(data->num_configs <= 0) {
        return data;
    }

    data->configs = (nost_config_data*)calloc(sizeof(nost_config_data), data->num_configs);

    if(data->configs == NULL) {
        return data;
    }

    for(c = 0, e_config = e_root->children; e_config; e_config = e_config->next) {
        if(strcmp((char*)e_config->name, "config")) {
            continue;
        }

        get_str_attr(e_config, "name", &data->configs[c].name);

        /* Default to n50 */
        data->configs[c].model = N50;
        get_int_attr(e_config, "model", (int*)&data->configs[c].model);

        for(m = 0, e_mode = e_config->children; e_mode; e_mode = e_mode->next) {
            if(strcmp((char*)e_mode->name, "mode")) {
                continue;
            }
            get_int_attr(e_mode, "num", &m);
                
            for(k = 0, e_key = e_mode->children; e_key; e_key = e_key->next) {
                if(strcmp((char*)e_key->name, "key")) {
                    continue;
                }
                get_int_attr(e_key, "num", &k);
                get_str_attr(e_key, "name", &data->configs[c].keys[m][k].name);
                get_int_attr(e_key, "type", (int*)&data->configs[c].keys[m][k].type);
                get_int_attr(e_key, "repeat", (int*)&data->configs[c].keys[m][k].repeat);
                get_int_attr(e_key, "delay", (int*)&data->configs[c].keys[m][k].repeat_delay);
                get_int_attr(e_key, "remote", (int*)&data->configs[c].keys[m][k].remote);

                for(s = 0, e_stroke = e_key->children; e_stroke; e_stroke = e_stroke->next) {
                    if(strcmp((char*)e_stroke->name, "stroke")) {
                        continue;
                    }
                    get_int_attr(e_stroke, "type", (int*)&data->configs[c].keys[m][k].data[s].type);
                    get_int_attr(e_stroke, "code", &data->configs[c].keys[m][k].data[s].code);
                    get_int_attr(e_stroke, "state", &data->configs[c].keys[m][k].data[s].state);
                    get_str_attr(e_stroke, "display", &data->configs[c].keys[m][k].data[s].display);
                    get_int_attr(e_stroke, "delay", &data->configs[c].keys[m][k].data[s].delay);
                    s++;
                }
                data->configs[c].keys[m][k].key_count = s;
                k++;
            }
            m++;
        }
        c++;
    }

    xmlFreeDoc(tree);
    xmlCleanupParser();

    return data;
}

