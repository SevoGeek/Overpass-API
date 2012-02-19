/** Copyright 2008, 2009, 2010, 2011, 2012 Roland Olbricht
*
* This file is part of Overpass_API.
*
* Overpass_API is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* Overpass_API is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Overpass_API.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>

#include <stdio.h>

#include "../../expat/expat_justparse_interface.h"
#include "../../template_db/random_file.h"
#include "../core/settings.h"
#include "../frontend/output.h"
#include "node_updater.h"
#include "way_updater.h"

using namespace std;

/**
 * Tests the library way_updater with a sample OSM file
 */

Node_Updater* node_updater;
Node current_node;
Way_Updater* way_updater;
Way current_way;
int state;
const int IN_NODES = 1;
const int IN_WAYS = 2;
ofstream* member_source_out;
ofstream* tags_source_out;
Osm_Backend_Callback* callback;

uint32 osm_element_count;

void start(const char *el, const char **attr)
{
  if (!strcmp(el, "tag"))
  {
    string key(""), value("");
    for (unsigned int i(0); attr[i]; i += 2)
    {
      if (!strcmp(attr[i], "k"))
	key = attr[i+1];
      if (!strcmp(attr[i], "v"))
	value = attr[i+1];
    }
    if (current_node.id > 0)
      current_node.tags.push_back(make_pair(key, value));
    else if (current_way.id > 0)
    {
      current_way.tags.push_back(make_pair(key, value));
      
      *tags_source_out<<current_way.id<<'\t'<<key<<'\t'<<value<<'\n';
    }
  }
  else if (!strcmp(el, "nd"))
  {
    if (current_way.id > 0)
    {
      unsigned int ref(0);
      for (unsigned int i(0); attr[i]; i += 2)
      {
	if (!strcmp(attr[i], "ref"))
	  ref = atoi(attr[i+1]);
      }
      current_way.nds.push_back(ref);
      
      *member_source_out<<ref<<' ';
    }
  }
  else if (!strcmp(el, "node"))
  {
    if (state == 0)
      state = IN_NODES;
    
    unsigned int id(0);
    double lat(100.0), lon(200.0);
    for (unsigned int i(0); attr[i]; i += 2)
    {
      if (!strcmp(attr[i], "id"))
	id = atoi(attr[i+1]);
      if (!strcmp(attr[i], "lat"))
	lat = atof(attr[i+1]);
      if (!strcmp(attr[i], "lon"))
	lon = atof(attr[i+1]);
    }
    current_node = Node(id, lat, lon);
  }
  else if (!strcmp(el, "way"))
  {
    if (state == IN_NODES)
    {
      callback->nodes_finished();
      node_updater->update(callback);
      callback->parser_started();
      osm_element_count = 0;
      state = IN_WAYS;
    }
    
    unsigned int id(0);
    for (unsigned int i(0); attr[i]; i += 2)
    {
      if (!strcmp(attr[i], "id"))
	id = atoi(attr[i+1]);
    }
    current_way = Way(id);
    
    *member_source_out<<id<<'\t';
  }
}

void end(const char *el)
{
  if (!strcmp(el, "node"))
  {
    node_updater->set_node(current_node);
    
    if (osm_element_count >= 4*1024*1024)
    {
      callback->node_elapsed(current_node.id);
      node_updater->update(callback, true);
      callback->parser_started();
      osm_element_count = 0;
    }
    current_node.id = 0;
  }
  else if (!strcmp(el, "way"))
  {
    way_updater->set_way(current_way);
    
    *member_source_out<<'\n';

    if (osm_element_count >= 4*1024*1024)
    {
      callback->way_elapsed(current_way.id);
      way_updater->update(callback, true);
      callback->parser_started();
      osm_element_count = 0;
    }
    current_way.id = 0;
  }
  ++osm_element_count;
}

void cleanup_files(const File_Properties& file_properties, string db_dir,
		   bool cleanup_map)
{
  remove((db_dir + file_properties.get_file_name_trunk() +
      file_properties.get_data_suffix() + file_properties.get_index_suffix()).c_str());
  remove((db_dir + file_properties.get_file_name_trunk() +
      file_properties.get_data_suffix()).c_str());
  
  if (cleanup_map)
  {
    remove((db_dir + file_properties.get_file_name_trunk() +
        file_properties.get_id_suffix()).c_str());
    remove((db_dir + file_properties.get_file_name_trunk() +
	file_properties.get_id_suffix() + file_properties.get_index_suffix()).c_str());
  }
}

int main(int argc, char* args[])
{
  string db_dir("./");
  
  try
  {
    ofstream member_db_out((db_dir + "member_db.csv").c_str());
    ofstream tags_local_out((db_dir + "tags_local.csv").c_str());
    ofstream tags_global_out((db_dir + "tags_global.csv").c_str());
    {
      Node_Updater node_updater_("./", false);
      node_updater = &node_updater_;
      Way_Updater way_updater_("./", false);
      way_updater = &way_updater_;
      
      member_source_out = new ofstream((db_dir + "member_source.csv").c_str());
      tags_source_out = new ofstream((db_dir + "tags_source.csv").c_str());
      callback = get_verbatim_callback();
      
      osm_element_count = 0;
      state = 0;
      //reading the main document
      callback->parser_started();
      parse(stdin, start, end);
      
      if (state == IN_NODES)
      {
	callback->nodes_finished();
	node_updater->update(callback);
      }
      else if (state == IN_WAYS)
      {
	callback->ways_finished();
	way_updater->update(callback);
      }
      
      delete member_source_out;
      delete tags_source_out;
    }

    Nonsynced_Transaction transaction(false, false, "./", "");
    
    // check update_members - compare both files for the result
    Block_Backend< Uint31_Index, Way_Skeleton > ways_db
	(transaction.data_index(osm_base_settings().WAYS));
    for (Block_Backend< Uint31_Index, Way_Skeleton >::Flat_Iterator
	 it(ways_db.flat_begin()); !(it == ways_db.flat_end()); ++it)
    {
      member_db_out<<it.object().id<<'\t';
      for (uint i(0); i < it.object().nds.size(); ++i)
	member_db_out<<it.object().nds[i]<<' ';
      member_db_out<<'\n';
    }
    
    // check update_way_tags_local - compare both files for the result
    Block_Backend< Tag_Index_Local, Uint32_Index > ways_local_db
	(transaction.data_index(osm_base_settings().WAY_TAGS_LOCAL));
    for (Block_Backend< Tag_Index_Local, Uint32_Index >::Flat_Iterator
	 it(ways_local_db.flat_begin()); !(it == ways_local_db.flat_end()); ++it)
    {
      tags_local_out<<it.object().val()<<'\t'
	  <<it.index().key<<'\t'<<it.index().value<<'\n';
    }
    
    // check update_way_tags_local - compare both files for the result
    Block_Backend< Tag_Index_Global, Uint32_Index > ways_global_db
	(transaction.data_index(osm_base_settings().WAY_TAGS_GLOBAL));
    for (Block_Backend< Tag_Index_Global, Uint32_Index >::Flat_Iterator
	 it(ways_global_db.flat_begin()); !(it == ways_global_db.flat_end()); ++it)
    {
      tags_global_out<<it.object().val()<<'\t'
	  <<it.index().key<<'\t'<<it.index().value<<'\n';
    }
  }
  catch (File_Error e)
  {
    report_file_error(e);
  }
  
  cleanup_files(*osm_base_settings().NODES, "./", true);
  cleanup_files(*osm_base_settings().NODE_TAGS_LOCAL, "./", true);
  cleanup_files(*osm_base_settings().NODE_TAGS_GLOBAL, "./", true);
  
  //cleanup_files(*osm_base_settings().WAYS, "./", true);
  //cleanup_files(*osm_base_settings().WAY_TAGS_LOCAL, "./", true);
  //cleanup_files(*osm_base_settings().WAY_TAGS_GLOBAL, "./", true);
  
  return 0;
}
