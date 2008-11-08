/*!
	@file
	@author		Albert Semenov
	@date		09/2008
	@module
*/
#include "MyGUI_ResourceManager.h"
#include "MyGUI_LanguageManager.h"
#include "MyGUI_XmlDocument.h"

namespace MyGUI
{

	const std::string XML_TYPE("Language");

	INSTANCE_IMPLEMENT(LanguageManager);

	void LanguageManager::initialise()
	{
		MYGUI_ASSERT(false == mIsInitialise, INSTANCE_TYPE_NAME << " initialised twice");
		MYGUI_LOG(Info, "* Initialise: " << INSTANCE_TYPE_NAME);

		ResourceManager::getInstance().registerLoadXmlDelegate(XML_TYPE) = newDelegate(this, &LanguageManager::_load);

		mCurrentLanguage = mMapFile.end();


		MYGUI_LOG(Info, INSTANCE_TYPE_NAME << " successfully initialized");
		mIsInitialise = true;
	}

	void LanguageManager::shutdown()
	{
		if (false == mIsInitialise) return;
		MYGUI_LOG(Info, "* Shutdown: " << INSTANCE_TYPE_NAME);

		ResourceManager::getInstance().unregisterLoadXmlDelegate(XML_TYPE);

		MYGUI_LOG(Info, INSTANCE_TYPE_NAME << " successfully shutdown");
		mIsInitialise = false;
	}

	bool LanguageManager::load(const std::string & _file, const std::string & _group)
	{
		return ResourceManager::getInstance()._loadImplement(_file, _group, true, XML_TYPE, INSTANCE_TYPE_NAME);
	}

	void LanguageManager::_load(xml::xmlNodePtr _node, const std::string & _file)
	{
		std::string def;

		// берем детей и крутимся, основной цикл
		xml::xmlNodeIterator root = _node->getNodeIterator();
		while (root.nextNode(XML_TYPE)) {

			// парсим атрибуты
			root->findAttribute("default", def);

			// берем детей и крутимся
			xml::xmlNodeIterator info = root->getNodeIterator();
			while (info.nextNode("Info")) {

				// парсим атрибуты
				std::string name(info->findAttribute("name"));

				// добавляем
				MapListString::iterator lang = mMapFile.find(name);
				if (lang == mMapFile.end()) {
					lang = mMapFile.insert(std::make_pair(name, VectorString())).first;
				}

				xml::xmlNodeIterator source_info = info->getNodeIterator();
				while (source_info.nextNode("Source")) {
					lang->second.push_back(source_info->getBody());
				};

			};
		};

		if ( ! def.empty()) setCurrentLanguage(def);
	}

	bool LanguageManager::setCurrentLanguage(const std::string & _name)
	{
		mCurrentLanguage = mMapFile.find(_name);
		if (mCurrentLanguage == mMapFile.end()) {
			MYGUI_LOG(Error, "Language '" << _name << "' is not found");
			return false;
		}

		loadLanguage(mCurrentLanguage->second, ResourceManager::getInstance().getResourceGroup());
		eventChangeLanguage(mCurrentLanguage->first);
		return true;
	}

	void LanguageManager::loadLanguage(const VectorString & _list, const std::string & _group)
	{
		mMapLanguage.clear();

		for (VectorString::const_iterator iter=_list.begin(); iter!=_list.end(); ++iter) {
			loadLanguage(*iter, _group);
		}
	}

	bool LanguageManager::loadLanguage(const std::string & _file, const std::string & _group)
	{

		if (!_group.empty()) {

			if (!helper::isFileExist(_file, _group)) {
				MYGUI_LOG(Error, "file '" << _file << "' not found in group'" << _group << "'");
				return false;
			}

			Ogre::DataStreamPtr stream = Ogre::ResourceGroupManager::getSingletonPtr()->openResource(_file, _group);

			// проверяем на сигнатуру utf8
			uint32 sign = 0;
			stream->read((void*)&sign, 3);
			if (sign != 0x00BFBBEF) {
				MYGUI_LOG(Error, "file '" << _file << "' is not UTF8 format");
				return false;
			}

			_loadLanguage(stream);
			return true;
		}

		std::ifstream stream(_file.c_str());
		if (false == stream.is_open()) {
			MYGUI_LOG(Error, "error open file '" << _file << "'");
			return false;
		}

		// проверяем на сигнатуру utf8
		uint32 sign = 0;
		stream.read((char*)&sign, 3);
		if (sign != 0x00BFBBEF) {
			MYGUI_LOG(Error, "file '" << _file << "' is not UTF8 format");
			stream.close();
			return false;
		}

		_loadLanguage(stream);
		stream.close();

		return true;
	}

	void LanguageManager::_loadLanguage(std::ifstream & _stream)
	{
		std::string read;
		while (false == _stream.eof()) {
			std::getline(_stream, read);
			if (read.empty()) continue;

			size_t pos = read.find_first_of(" \t");
			if (pos == std::string::npos) mMapLanguage[read] = "";
			else mMapLanguage[read.substr(0, pos)] = read.substr(pos+1, std::string::npos);
		};
	}

	void LanguageManager::_loadLanguage(const Ogre::DataStreamPtr& stream)
	{
		std::string read;
		while (false == stream->eof()) {
			read = stream->getLine (false);
			if (read.empty()) continue;

			size_t pos = read.find_first_of(" \t");
			if (pos == std::string::npos) mMapLanguage[read] = "";
			else mMapLanguage[read.substr(0, pos)] = read.substr(pos+1, std::string::npos);
		};
	}

	Ogre::UTFString LanguageManager::replaceTags(const Ogre::UTFString & _line)
	{
		// вот хз, что быстрее, итераторы или математика указателей,
		// для непонятно какого размера одного символа UTF8
		Ogre::UTFString line(_line);

		if (mMapLanguage.empty()) return _line;

		Ogre::UTFString::iterator end = line.end();
		for (Ogre::UTFString::iterator iter=line.begin(); iter!=end; ++iter) {
			if (*iter == '#') {
				++iter;
				if (iter == end) {
					return line;
				}
				else {
					if (*iter != '{') continue;
					Ogre::UTFString::iterator iter2 = iter;
					++iter2;
					while (true) {
						if (iter2 == end) return line;
						if (*iter2 == '}') {

							size_t start = iter - line.begin();
							size_t len = (iter2 - line.begin()) - start - 1;
							const Ogre::UTFString & tag = line.substr(start + 1, len);

							MapLanguageString::iterator replace = mMapLanguage.find(tag);
							if (replace == mMapLanguage.end()) {
								iter = line.insert(iter, '#') + size_t(len + 2);
								end = line.end();
								break;
							}
							else {
								iter = line.erase(iter - size_t(1), iter2 + size_t(1));
								size_t pos = iter - line.begin();
								line.insert(pos, replace->second);
								iter = line.begin() + pos + replace->second.length();
								end = line.end();
								if (iter == end) return line;
								break;
							}

							iter = iter2;
							break;
						}
						++iter2;
					};
				}
			}
		}

		return line;
	}

	Ogre::UTFString LanguageManager::getTag(const Ogre::UTFString & _tag)
	{
		MapLanguageString::iterator iter = mMapLanguage.find(_tag);
		if (iter == mMapLanguage.end()) {
			return _tag;
		}
		return iter->second;
	}

} // namespace MyGUI
