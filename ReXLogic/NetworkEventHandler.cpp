// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "NetworkEventHandler.h"
#include "NetInMessage.h"
#include "RexProtocolMsgIDs.h"
#include "OpenSimProtocolModule.h"
#include "RexLogicModule.h"

#include "EC_Viewable.h"
#include "EC_FreeData.h"
#include "EC_SpatialSound.h"
#include "EC_OpenSimPrim.h"

namespace RexLogic
{
    NetworkEventHandler::NetworkEventHandler(Foundation::Framework *framework)
    {
        framework_ = framework;
    }

    NetworkEventHandler::~NetworkEventHandler()
    {

    }

    bool NetworkEventHandler::HandleOpenSimNetworkEvent(Core::event_id_t event_id, Foundation::EventDataInterface* data)
    {
        if(event_id == OpenSimProtocol::EVENT_NETWORK_IN)
        {
            OpenSimProtocol::NetworkEventInboundData *netdata = static_cast<OpenSimProtocol::NetworkEventInboundData *>(data);
            switch(netdata->messageID)
            {
                case RexNetMsgGenericMessage:           return HandleOSNE_GenericMessage(netdata); break;
                case RexNetMsgObjectDescription:        return HandleOSNE_ObjectDescription(netdata); break; 
                case RexNetMsgObjectName:               return HandleOSNE_ObjectName(netdata); break;
                case RexNetMsgObjectUpdate:             return HandleOSNE_ObjectUpdate(netdata); break;
                default:                                return false; break;
            }
        }
        return false;
    }

    Foundation::EntityPtr NetworkEventHandler::GetPrimEntitySafe(Core::entity_id_t entityid)
    {
        Foundation::SceneManagerServiceInterface *sceneManager = framework_->GetService<Foundation::SceneManagerServiceInterface>(Foundation::Service::ST_SceneManager);
        Foundation::ScenePtr scene = sceneManager->GetScene("World");

        // TODO tucofixme, how to make sure this is a prim entity?
        if (!scene->HasEntity(entityid))
            return CreateNewPrimEntity(entityid);
        else
            return scene->GetEntity(entityid);
    }
  
    Foundation::EntityPtr NetworkEventHandler::GetPrimEntitySafe(Core::entity_id_t entityid, RexUUID fullid)
    {
        Foundation::SceneManagerServiceInterface *sceneManager = framework_->GetService<Foundation::SceneManagerServiceInterface>(Foundation::Service::ST_SceneManager);
        Foundation::ScenePtr scene = sceneManager->GetScene("World");

        // TODO tucofixme, how to make sure this is a prim entity?
        if (!scene->HasEntity(entityid))
        {
            UUIDs_[fullid] = entityid;
            return CreateNewPrimEntity(entityid);
        }
        else
            return scene->GetEntity(entityid);
    }  
   
    Foundation::EntityPtr NetworkEventHandler::GetPrimEntity(RexUUID entityuuid)
    {
        Foundation::SceneManagerServiceInterface *sceneManager = framework_->GetService<Foundation::SceneManagerServiceInterface>(Foundation::Service::ST_SceneManager);
        Foundation::ScenePtr scene = sceneManager->GetScene("World");
        
        if(UUIDs_.find(entityuuid) != UUIDs_.end())
        {
            return scene->GetEntity(UUIDs_[entityuuid]);
        }
        else
        {
            Foundation::EntityPtr emptyptr;
            return emptyptr; 
        }        
    }    
    
    
    Foundation::EntityPtr NetworkEventHandler::CreateNewPrimEntity(Core::entity_id_t entityid)
    {
        Foundation::SceneManagerServiceInterface *sceneManager = framework_->GetService<Foundation::SceneManagerServiceInterface>(Foundation::Service::ST_SceneManager);
        Foundation::ScenePtr scene = sceneManager->GetScene("World");
        
        Core::StringVector defaultcomponents;
        defaultcomponents.push_back(EC_OpenSimPrim::NameStatic());
        defaultcomponents.push_back(EC_Viewable::NameStatic());
        
        Foundation::EntityPtr entity = scene->CreateEntity(entityid,defaultcomponents); 
        return entity;
    }
    
    Foundation::EntityPtr NetworkEventHandler::GetAvatarEntitySafe(Core::entity_id_t entityid)
    {
        Foundation::SceneManagerServiceInterface *sceneManager = framework_->GetService<Foundation::SceneManagerServiceInterface>(Foundation::Service::ST_SceneManager);
        Foundation::ScenePtr scene = sceneManager->GetScene("World");

        // TODO tucofixme, how to make sure this is a avatar entity?
        if (!scene->HasEntity(entityid))
            return CreateNewAvatarEntity(entityid);
        else
            return scene->GetEntity(entityid);
    }    

    Foundation::EntityPtr NetworkEventHandler::CreateNewAvatarEntity(Core::entity_id_t entityid)
    {
        Foundation::SceneManagerServiceInterface *sceneManager = framework_->GetService<Foundation::SceneManagerServiceInterface>(Foundation::Service::ST_SceneManager);
        Foundation::ScenePtr scene = sceneManager->GetScene("World");
        
        Core::StringVector defaultcomponents;
        // TODO tucofixme, add avatar default components
        
        Foundation::EntityPtr entity = scene->CreateEntity(entityid,defaultcomponents); 
        return entity;
    }


    bool NetworkEventHandler::HandleOSNE_ObjectUpdate(OpenSimProtocol::NetworkEventInboundData* data)
    {
        NetInMessage *msg = data->message;
 
        uint64_t regionhandle = msg->ReadU64();
        msg->SkipToNextVariable(); // TimeDilation U16

        uint32_t localid = msg->ReadU32(); 
        msg->SkipToNextVariable();		// State U8
        RexUUID fullid = msg->ReadUUID();
        msg->SkipToNextVariable();		// CRC U32
        uint8_t pcode = msg->ReadU8();
        
        Foundation::EntityPtr entity;
        switch(pcode)
        {
            // Prim
            case 0x09:
                entity = GetPrimEntitySafe(localid,fullid);                
                if(entity)
                {
                    Foundation::ComponentInterfacePtr component = entity->GetComponent("EC_OpenSimPrim");
                    static_cast<EC_OpenSimPrim*>(component.get())->HandleObjectUpdate(data);
                }
                break;
            // Avatar                
            case 0x2f:
                entity = GetAvatarEntitySafe(localid);
                // TODO tucofixme, set values to component      
                break;
        }
        return false;
    }

    bool NetworkEventHandler::HandleOSNE_ObjectName(OpenSimProtocol::NetworkEventInboundData* data)
    {
        NetInMessage *msg = data->message;
    
        msg->ResetReading();
        msg->SkipToFirstVariableByName("LocalID");
        uint32_t localid = msg->ReadU32();
        
        Foundation::EntityPtr entity = GetPrimEntitySafe(localid);
        if(entity)
        {
            Foundation::ComponentInterfacePtr component = entity->GetComponent("EC_OpenSimPrim");
            static_cast<EC_OpenSimPrim*>(component.get())->HandleObjectName(data);        
        }
        return false;
    }

    bool NetworkEventHandler::HandleOSNE_ObjectDescription(OpenSimProtocol::NetworkEventInboundData* data)
    {
        NetInMessage *msg = data->message;
    
        msg->ResetReading();
        msg->SkipToFirstVariableByName("LocalID");
        uint32_t localid = msg->ReadU32();
        
        Foundation::EntityPtr entity = GetPrimEntitySafe(localid);
        if(entity)
        {
            Foundation::ComponentInterfacePtr component = entity->GetComponent("EC_OpenSimPrim");
            static_cast<EC_OpenSimPrim*>(component.get())->HandleObjectDescription(data);        
        }
        return false;     
    }


    bool NetworkEventHandler::HandleOSNE_GenericMessage(OpenSimProtocol::NetworkEventInboundData* data)
    {
        size_t bytes_read;
        
        data->message->ResetReading();    
        data->message->SkipToNextVariable();      // AgentId
        data->message->SkipToNextVariable();      // SessionId
        data->message->SkipToNextVariable();      // TransactionId
        
        const uint8_t *methodnamebytes = data->message->ReadBuffer(&bytes_read);
        if(bytes_read > 0)
        {
            std::string methodname = (const char *)methodnamebytes;
            if(methodname == "RexMediaUrl")
                return HandleRexGM_RexMediaUrl(data);
            else if(methodname == "RexPrimData")
                return HandleRexGM_RexPrimData(data); 
        }    
        return false;    
    }

    bool NetworkEventHandler::HandleRexGM_RexMediaUrl(OpenSimProtocol::NetworkEventInboundData* data)
    {
        // TODO tucofixme
        return false;
    }

    bool NetworkEventHandler::HandleRexGM_RexPrimData(OpenSimProtocol::NetworkEventInboundData* data)
    {
        size_t bytes_read;    
        data->message->ResetReading();
        data->message->SkipToFirstVariableByName("Parameter");           
        
        const uint8_t *readbytedata;      
        readbytedata = data->message->ReadBuffer(&bytes_read);
        RexUUID primuuid = *(RexUUID*)(&readbytedata[0]);

        Foundation::EntityPtr entity = GetPrimEntity(primuuid);
        if(entity)
        {
            Foundation::ComponentInterfacePtr oscomponent = entity->GetComponent("EC_OpenSimPrim");
            static_cast<EC_OpenSimPrim*>(oscomponent.get())->HandleRexPrimData(data);

            Foundation::ComponentInterfacePtr viewcomponent = entity->GetComponent("EC_Viewable");
            static_cast<EC_Viewable*>(viewcomponent.get())->HandleRexPrimData(data);
        }
        return false;
    }
}
