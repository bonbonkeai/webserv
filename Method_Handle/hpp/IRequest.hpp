#endif

业务处理的统一接口：
virtual void handle(const EffectiveConfig&, Client&) = 0;
所有请求类都会实现它->get/post/delete。