
//=============================================
//���������ļ�
//=============================================


#include <sstream>
#include <string>

//������IP��ʽת��Ϊ����
unsigned int stringtoint(const std::string& ipAddress) {
    std::istringstream iss(ipAddress);
    std::string token;
    unsigned int result = 0;
    int shift = 24;

    while (std::getline(iss, token, '.')) {
        result |= (std::stoi(token) << shift);
        shift -= 8;
    }

    return result;
}


