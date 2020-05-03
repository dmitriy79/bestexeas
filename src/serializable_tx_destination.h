#ifndef SERIALIZABLE_TX_DESTINATION_H
#define SERIALIZABLE_TX_DESTINATION_H

#include "script.h"
#include "serialize.h"

class CSerializableTxDestination {
    enum {
        TYPE_NO = 0,
        TYPE_KEY = 1,
        TYPE_SCRIPT = 2,
    };
    class CDestinationGetTypeVisitor : public boost::static_visitor<int>
    {
        public:
            CDestinationGetTypeVisitor()
            {
            }

            int operator()(const CNoDestination& dest) const {
                return TYPE_NO;
            }

            int operator()(const CKeyID& keyID) const {
                return TYPE_KEY;
            }

            int operator()(const CScriptID& scriptID) const {
                return TYPE_SCRIPT;
            }
    };

    private:
    CTxDestination destination;

    public:
    CSerializableTxDestination()
    {
    }

    CSerializableTxDestination(const CTxDestination& destination)
        : destination(destination)
    {
    }

    CSerializableTxDestination(const CNoDestination& destination)
        : destination(destination)
    {
    }

    CSerializableTxDestination(const CKeyID& destination)
        : destination(destination)
    {
    }

    CSerializableTxDestination(const CScriptID& destination)
        : destination(destination)
    {
    }

    template<typename Stream>
    unsigned int ReadWriteTxDestination(Stream& s, int nType, int nVersion, CSerActionGetSerializeSize ser_action) const
    {
        unsigned int nSize = 0;
        int cType = boost::apply_visitor(CDestinationGetTypeVisitor(), destination);
        nSize += ::SerReadWrite(s, cType, nType, nVersion, ser_action);
        switch (cType) {
            case TYPE_NO:
                break;
            case TYPE_KEY:
                {
                    const CKeyID& keyID = boost::get<CKeyID>(destination);
                    nSize += ::SerReadWrite(s, keyID, nType, nVersion, ser_action);
                }
                break;
            case TYPE_SCRIPT:
                {
                    const CScriptID& scriptID = boost::get<CScriptID>(destination);
                    nSize += ::SerReadWrite(s, scriptID, nType, nVersion, ser_action);
                }
                break;
        }
        return nSize;
    }

    template<typename Stream>
    unsigned int ReadWriteTxDestination(Stream& s, int nType, int nVersion, CSerActionSerialize ser_action) const
    {
        unsigned int nSize = 0;
        int cType = boost::apply_visitor(CDestinationGetTypeVisitor(), destination);
        nSize += ::SerReadWrite(s, cType, nType, nVersion, ser_action);
        switch (cType) {
            case TYPE_NO:
                break;
            case TYPE_KEY:
                {
                    const CKeyID& keyID = boost::get<CKeyID>(destination);
                    nSize += ::SerReadWrite(s, keyID, nType, nVersion, ser_action);
                }
                break;
            case TYPE_SCRIPT:
                {
                    const CScriptID& scriptID = boost::get<CScriptID>(destination);
                    nSize += ::SerReadWrite(s, scriptID, nType, nVersion, ser_action);
                }
                break;
        }
        return nSize;
    }

    template<typename Stream>
    unsigned int ReadWriteTxDestination(Stream& s, int nType, int nVersion, CSerActionUnserialize ser_action)
    {
        unsigned int nSize = 0;
        int cType;
        nSize += ::SerReadWrite(s, cType, nType, nVersion, ser_action);
        switch (cType) {
            case TYPE_NO:
                break;
            case TYPE_KEY:
                {
                    CKeyID keyID;
                    nSize += ::SerReadWrite(s, keyID, nType, nVersion, ser_action);
                    destination = keyID;
                }
                break;
            case TYPE_SCRIPT:
                {
                    CScriptID scriptID;
                    nSize += ::SerReadWrite(s, scriptID, nType, nVersion, ser_action);
                    destination = scriptID;
                }
                break;
        }
        return nSize;
    }

    IMPLEMENT_SERIALIZE
    (
      ReadWriteTxDestination(s, nType, nVersion, ser_action);
    )

    const CTxDestination& GetTxDestination() const
    {
        return destination;
    }
};

#endif // SERIALIZABLE_TX_DESTINATION_H
