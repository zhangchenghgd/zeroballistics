

#include "ObjectParts.h"

#include <boost/filesystem.hpp>

#include "assert.h"
#include "Paths.h"
#include "Utils.h"
#include "Log.h"



//------------------------------------------------------------------------------
/**
 *  Returns all models that are in the model subdir "obj_name", or
 *  simply obj_name if no such subdir exists.
 *
 *  Appendices are tried in order of occurence, " " is for no appendix.
 *
 *  If no direct match is found, "-XXX" is repeatatively stripped to
 *  find any wildcard matches (ending with +).
 *
 */
std::vector<std::string> getObjectPartNames(const std::string & obj_name,
                                            const std::string & appendix_list)
{
    assert(!appendix_list.empty());
    
    using namespace boost::filesystem;
        
    std::vector<std::string> part_names;

    try
    {
        for (unsigned a=0; a<appendix_list.size(); ++a)
        {
            std::string cur_full_name = obj_name;
            if (appendix_list[a] != ' ')
            {
                cur_full_name += "- ";
                *cur_full_name.rbegin() = appendix_list[a];
            }

            path model_path(MODEL_PATH);
            model_path /= cur_full_name;
        
            try
            {
                for (directory_iterator it(model_path);
                     it != directory_iterator();
                     ++it)
                {
                    std::string name = it->path().leaf();

                    // only handle xml, not bbm files.
                    if (name.rfind(".xml") != name.length()-4) continue;

                    // strip .xml suffix
                    part_names.push_back(cur_full_name + "/" + name.substr(0, name.length()-4));
                }
            } catch (basic_filesystem_error<path> & e)
            {
                // directory listing cannot be done, assume single file object
                if (!existsFile((MODEL_PATH + "/" + cur_full_name + ".xml").c_str())) continue;
                part_names.push_back(cur_full_name);
            }

            // order is unspecified when enumerating directory!
            std::sort(part_names.begin(), part_names.end());
        
            return part_names;
        }


        // No direct match could be found, search for wildcard matches.
        std::vector<std::string> possible_matches;
        path model_path(MODEL_PATH);
        for (directory_iterator it(model_path);
             it != directory_iterator();
             ++it)
        {
            std::string name = it->path().leaf();
            if (name.rfind(".xml") == name.length()-4)
            {
                name = name.substr(0,name.length()-4);
            }
            if (*name.rbegin() == '+')
            {
                possible_matches.push_back(name.substr(0, name.length()-1));
            }
        }

        std::string cur_name = obj_name;
        std::size_t sep_pos;
        for(;;)
        {
            for (unsigned w=0; w<possible_matches.size(); ++w)
            {
                if (possible_matches[w] == cur_name)
                {
                    return getObjectPartNames(cur_name + '+');
                }
            }

            sep_pos = cur_name.rfind('-');
            if (sep_pos == 0 ||
                sep_pos == std::string::npos)
            {
                break;
            }
            cur_name = cur_name.substr(0, sep_pos);  
        }
    } catch (basic_filesystem_error<path> & e)
    {
        s_log << Log::warning
              << "GameLogicServer::getObjectPartNames("
              << obj_name
              << ", "
              << appendix_list
              << ": "
              << e.what()
              << "\n";
    }

    return part_names;
}

