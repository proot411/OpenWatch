#include "vault.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <memory>
#include <stdexcept>
namespace {
constexpr int Limit=4*1024*1024;
struct Key {unsigned char bytes[32];~Key(){OPENSSL_cleanse(bytes,sizeof bytes);}};
void check(bool ok){if(!ok)throw std::runtime_error("Device file could not be unlocked: wrong passphrase, damaged file, or unsupported format.");}
void derive(Key &key,const QByteArray &salt,const QString &password){auto utf=password.toUtf8();bool ok=PKCS5_PBKDF2_HMAC(utf.constData(),utf.size(),reinterpret_cast<const unsigned char*>(salt.constData()),salt.size(),600000,EVP_sha256(),32,key.bytes)==1;OPENSSL_cleanse(utf.data(),utf.size());check(ok);}
}
namespace vault {
QByteArray seal(const QByteArray &plain,const QString &password){
 check(plain.size()<=Limit && !password.isEmpty());QByteArray header("OWV1",4);QByteArray random(28,0);check(RAND_bytes(reinterpret_cast<unsigned char*>(random.data()),random.size())==1);header+=random;
 Key key;derive(key,header.mid(4,16),password);
 std::unique_ptr<EVP_CIPHER_CTX,decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(),EVP_CIPHER_CTX_free);check(bool(ctx));
 check(EVP_EncryptInit_ex(ctx.get(),EVP_aes_256_gcm(),nullptr,key.bytes,reinterpret_cast<const unsigned char*>(header.constData()+20))==1);
 int n=0;check(EVP_EncryptUpdate(ctx.get(),nullptr,&n,reinterpret_cast<const unsigned char*>(header.constData()),header.size())==1);
 QByteArray encrypted(plain.size()+16,0);check(EVP_EncryptUpdate(ctx.get(),reinterpret_cast<unsigned char*>(encrypted.data()),&n,reinterpret_cast<const unsigned char*>(plain.constData()),plain.size())==1);int total=n;
 check(EVP_EncryptFinal_ex(ctx.get(),reinterpret_cast<unsigned char*>(encrypted.data()+total),&n)==1);encrypted.resize(total+n);QByteArray tag(16,0);check(EVP_CIPHER_CTX_ctrl(ctx.get(),EVP_CTRL_GCM_GET_TAG,16,tag.data())==1);return header+encrypted+tag;
}
QByteArray open(const QByteArray &data,const QString &password){
 check(data.size()>=48 && data.size()<=Limit+48 && data.startsWith("OWV1"));Key key;derive(key,data.mid(4,16),password);
 std::unique_ptr<EVP_CIPHER_CTX,decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(),EVP_CIPHER_CTX_free);check(bool(ctx));
 check(EVP_DecryptInit_ex(ctx.get(),EVP_aes_256_gcm(),nullptr,key.bytes,reinterpret_cast<const unsigned char*>(data.constData()+20))==1);int n=0;
 check(EVP_DecryptUpdate(ctx.get(),nullptr,&n,reinterpret_cast<const unsigned char*>(data.constData()),32)==1);QByteArray plain(data.size(),0);
 check(EVP_DecryptUpdate(ctx.get(),reinterpret_cast<unsigned char*>(plain.data()),&n,reinterpret_cast<const unsigned char*>(data.constData()+32),data.size()-48)==1);int total=n;auto tag=data.right(16);check(EVP_CIPHER_CTX_ctrl(ctx.get(),EVP_CTRL_GCM_SET_TAG,16,tag.data())==1);
 if(EVP_DecryptFinal_ex(ctx.get(),reinterpret_cast<unsigned char*>(plain.data()+total),&n)!=1){OPENSSL_cleanse(plain.data(),plain.size());check(false);}plain.resize(total+n);return plain;
}
}
