#ifndef SMARTPTR_PROXY_H
#define SMARTPTR_PROXY_H

void smartptr_proxy_init(const char* mod_name, const sipAPIDef *apidef);
void smartptr_proxy_post_init(PyObject *module_dict);

#endif // SMARTPTR_PROXY_H
