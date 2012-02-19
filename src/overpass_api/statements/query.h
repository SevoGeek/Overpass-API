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

#ifndef DE__OSM3S___OVERPASS_API__STATEMENTS__QUERY_H
#define DE__OSM3S___OVERPASS_API__STATEMENTS__QUERY_H

#include <map>
#include <string>
#include <vector>
#include "statement.h"

using namespace std;

const int QUERY_NODE = 1;
const int QUERY_WAY = 2;
const int QUERY_RELATION = 3;
// const int QUERY_AREA = 4;

class Regular_Expression;

class Query_Statement : public Statement
{
  public:
    Query_Statement(int line_number_, const map< string, string >& input_attributes);
    virtual ~Query_Statement() {}
    virtual void add_statement(Statement* statement, string text);
    virtual string get_name() const { return "query"; }
    virtual string get_result_name() const { return output; }
    virtual void forecast();
    virtual void execute(Resource_Manager& rman);
    
    static Generic_Statement_Maker< Query_Statement > statement_maker;
    
  private:
    string output;
    int type;
    vector< string > keys;    
    vector< pair< string, string > > key_values;    
    vector< pair< string, Regular_Expression* > > key_regexes;    
    vector< pair< string, string > > key_nvalues;    
    vector< pair< string, Regular_Expression* > > key_nregexes;    
    vector< Query_Constraint* > constraints;
    
    vector< uint32 > collect_ids
        (const File_Properties& file_prop, Resource_Manager& rman);
	 
    template < typename TIndex, typename TObject >
    void get_elements_by_id_from_db
        (map< TIndex, vector< TObject > >& elements,
	 const vector< uint32 >& ids, const set< pair< TIndex, TIndex > >& range_req,
         Resource_Manager& rman, File_Properties& file_prop);
	 
    template < typename TIndex >
    set< pair< TIndex, TIndex > > get_ranges_by_id_from_db
        (const vector< uint32 >& ids,
         Resource_Manager& rman, File_Properties& file_prop);
};

class Has_Kv_Statement : public Statement
{
  public:
    Has_Kv_Statement(int line_number_, const map< string, string >& input_attributes);
    virtual string get_name() const { return "has-kv"; }
    virtual string get_result_name() const { return ""; }
    virtual void forecast();
    virtual void execute(Resource_Manager& rman) {}
    virtual ~Has_Kv_Statement();
    
    static Generic_Statement_Maker< Has_Kv_Statement > statement_maker;
    
    string get_key() const { return key; }
    string get_value() const { return value; }
    Regular_Expression* get_regex() { return regex; }
    bool get_straight() const { return straight; }
    
  private:
    string key, value;
    Regular_Expression* regex;
    bool straight;
};

#endif
