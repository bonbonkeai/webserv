#ifndef POSTREQUEST_HPP
#define POSTREQUEST_HPP

#include "Method_Handle/hpp/IRequest.hpp"

class PostRequest : public IRequest
{
private:
        HTTPRequest _req;
        HTTPResponse handleRawUploadFallback(const EffectiveConfig& cfg);
public:
        PostRequest(const HTTPRequest& req);
        virtual ~PostRequest();

        virtual HTTPResponse handle(const EffectiveConfig& cfg);
};

#endif

/*处理 POST
若是上传路径 → UploadHandler
若是 CGI → 进入 CGI
若不允许 → ErrorRequest*/
