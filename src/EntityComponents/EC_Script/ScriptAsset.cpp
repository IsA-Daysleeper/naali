#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include <boost/regex.hpp>
#include <QList>
#include <QDir>
#include <QTextStream>
#include "LoggingFunctions.h"
#include "MemoryLeakCheck.h"

#include "ScriptAsset.h"
#include "AssetAPI.h"

ScriptAsset::~ScriptAsset()
{
    Unload();
}

void ScriptAsset::DoUnload()
{
    scriptContent = "";
    references.clear();
}

bool ScriptAsset::DeserializeFromData(const u8 *data, size_t numBytes, const bool allowAsynchronous)
{
    QByteArray arr((const char *)data, numBytes);
    scriptContent = arr;

    ParseReferences();
    assetAPI->AssetLoadCompleted(Name());
    return true;
}

bool ScriptAsset::SerializeTo(std::vector<u8> &dst, const QString &serializationParameters) const
{
    QByteArray arr(scriptContent.toStdString().c_str());
    dst.clear();
    dst.insert(dst.end(), arr.data(), arr.data() + arr.size());
    return true;
}

void ScriptAsset::ParseReferences()
{
    references.clear();
    QStringList addedRefs;
    std::string content = scriptContent.toStdString();
    boost::sregex_iterator searchEnd;

    // In headless mode we don't want to mark certain asset types as
    // dependencies for the script, as they will fail Load() anyways
    QStringList ignoredAssetTypes;
    if (assetAPI->IsHeadless())
        ignoredAssetTypes << "QtUiFile" << "Texture" << "OgreParticle" << "OgreMaterial" << "Audio";

    // Script asset dependencies are expressed in code comments using lines like "// !ref: http://myserver.com/myasset.png".
    // The asset type can be specified using a comma: "// !ref: http://myserver.com/avatarasset.xml, Avatar".
    boost::regex expression("!ref:\\s*(.*?)(\\s*,\\s*(.*?))?\\s*(\\n|$)");
    for(boost::sregex_iterator iter(content.begin(), content.end(), expression); iter != searchEnd; ++iter)
    {
        AssetReference ref;
        ref.ref = assetAPI->ResolveAssetRef(Name(), (*iter)[1].str().c_str());
        if ((*iter)[3].matched)
            ref.type = (*iter)[3].str().c_str();
        
        if (ignoredAssetTypes.contains(AssetAPI::GetResourceTypeFromAssetRef(ref.ref)))
            continue;
        if (!addedRefs.contains(ref.ref, Qt::CaseInsensitive))
        {
            references.push_back(ref);
            addedRefs << ref.ref;
        }
    }

    // Include dependencies: eg. local://myother.js or http://server.com/myother.js
    expression = boost::regex("engine.IncludeFile\\(\\s*\"\\s*(.*?)\\s*\"\\s*\\)");
    for(boost::sregex_iterator iter(content.begin(), content.end(), expression); iter != searchEnd; ++iter)
    {
        // Ask AssetAPI to resolve the ref
        AssetReference ref;
        ref.ref = assetAPI->ResolveAssetRef(Name(), (*iter)[1].str().c_str());
        if (!addedRefs.contains(ref.ref, Qt::CaseInsensitive))
        {
            references.push_back(ref);
            addedRefs << ref.ref;
        }
    }

    // Search for the first trusted request with optional reason for the request. 
    // The reason can be used in the UI when requesting user from the trusted status.
    // !trusted: Optional info what this script does.
    // @todo This could be made to perform a bit faster with boost regexp at some point.
    // @note We use '// !trusted:' even if there is no message following it, this is to
    // not hit false positives in code eg. 'var trusted = false; if (!trusted)' etc.
    if (scriptContent.contains("// !trusted:", Qt::CaseInsensitive))
    {
        QTextStream s(&scriptContent, QIODevice::ReadOnly);
        while (!s.atEnd())
        {
            QString line = s.readLine();
            QString searchTerm = "// !trusted:";
            if (!line.contains(searchTerm, Qt::CaseInsensitive))
                continue;
            int sIndex = line.indexOf(searchTerm, 0, Qt::CaseInsensitive);
            if (sIndex == -1)
                continue;
            sIndex += searchTerm.size();
            trustRequestReason = line.mid(sIndex).trimmed();
            trustRequested = true;
            break;
        }
    }
}

bool ScriptAsset::IsLoaded() const
{
    return !scriptContent.isEmpty();
}
