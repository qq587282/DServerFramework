#include <string>

#include "WebSocketFormat.h"
#include "UseWebPacketSingleNetSession.h"

UseWebPacketSingleNetSession::UseWebPacketSingleNetSession()
{
    mShakehanded = false;
}

size_t UseWebPacketSingleNetSession::onMsg(const char* buffer, size_t len)
{
    if (!mShakehanded)
    {
        const static char* SEC_FLAG = "Sec-WebSocket-Key: ";
        const static int SEC_LEN = sizeof("Sec-WebSocket-Key: ") - 1;

        /*  处理websocket握手    */
        const char* f = strstr(buffer, SEC_FLAG);
        if (f != nullptr)
        {
            const char* fend = strstr(f + SEC_LEN, "\r\n");
            if (fend != nullptr)
            {
                mShakehanded = true;
                const char* keyStart = f + SEC_LEN;

                std::string response = WebSocketFormat::wsHandshake(std::string(keyStart, fend-keyStart));
                sendPacket(response.c_str(), response.size());
                return len;
            }
        }
        
        return 0;
    }
    else
    {
        /*  处理websocket frame   */

        const char* parse_str = buffer;
        size_t total_proc_len = 0;
        PACKET_LEN_TYPE left_len = static_cast<PACKET_LEN_TYPE>(len);

        static size_t WEB_PACKET_HEAD_LEN = 14;

        while (true)
        {
            bool flag = false;
            if (left_len >= WEB_PACKET_HEAD_LEN)
            {
                mParsePayload.clear();

                auto opcode = WebSocketFormat::WebSocketFrameType::ERROR_FRAME;
                size_t frameSize = 0;
                bool isFin = false;
                if (WebSocketFormat::wsFrameExtractBuffer(parse_str, left_len, mParsePayload, opcode, frameSize, isFin))
                {
                    if (isFin && (opcode == WebSocketFormat::WebSocketFrameType::TEXT_FRAME || opcode == WebSocketFormat::WebSocketFrameType::BINARY_FRAME))
                    {
                        if (!mCacheFrame.empty())
                        {
                            mCacheFrame += mParsePayload;
                            mParsePayload = std::move(mCacheFrame);
                            mCacheFrame.clear();
                        }

                        /*  只有text和binary帧,才解析数据    */
                        BasePacketReader rp(mParsePayload.c_str(), mParsePayload.size());
                        rp.readINT8();
                        auto packetLen = rp.readUINT32();
                        
                        assert(packetLen >= WEB_PACKET_HEAD_LEN && packetLen == mParsePayload.size());
                        if (packetLen >= WEB_PACKET_HEAD_LEN && packetLen == mParsePayload.size())
                        {
                            auto cmd = rp.readINT32();
                            rp.readINT32(); /*  serial id   */
                            const char* body = mParsePayload.c_str() + rp.getPos();
                            rp.addPos(static_cast<int>(packetLen) - static_cast<int>(WEB_PACKET_HEAD_LEN));
                            rp.readINT8();

                            procPacket(cmd, body, packetLen - static_cast<uint32_t>(WEB_PACKET_HEAD_LEN));
                        }
                    }
                    else if (opcode == WebSocketFormat::WebSocketFrameType::CONTINUATION_FRAME)
                    {
                        mCacheFrame += mParsePayload;
                    }
                    else if (opcode == WebSocketFormat::WebSocketFrameType::PING_FRAME)
                    {
                    }
                    else if (opcode == WebSocketFormat::WebSocketFrameType::PONG_FRAME)
                    {
                    }
                    else
                    {
                        assert(false);
                    }

                    total_proc_len += frameSize;
                    parse_str += frameSize;
                    left_len -= static_cast<PACKET_LEN_TYPE>(frameSize);

                    flag = true;
                }
            }

            if (!flag)
            {
                break;
            }
        }

        return total_proc_len;
    }

    return 0;
}