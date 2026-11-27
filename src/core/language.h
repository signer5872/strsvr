#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <QObject>
#include <QString>
#include <QHash>

class QTranslator;

/**
 * 运行时中英文切换。
 *
 * 不需要 .ts/.qm 文件：内置一张 英文 -> 中文 的词典，
 * 通过自定义 QTranslator 挂到 qApp 上，因此代码里原有的 tr("...")
 * 无需改动即可切换到中文。
 */
class LanguageManager : public QObject
{
    Q_OBJECT
public:
    enum Language { English = 0, Chinese = 1 };

    static LanguageManager &instance();

    Language current() const { return m_lang; }
    bool isChinese() const { return m_lang == Chinese; }

    /** 切换语言：保存到配置并立即生效（新创建的对话框会使用新语言）。 */
    void setLang(Language lang);

    /** 启动时按配置文件中的 set/language 安装翻译器。 */
    void applyFromConfig();

    /** 供 tr() 之外动态生成的文本使用（词典里没有则原样返回）。 */
    QString text(const QString &english) const;

    /** 内置的中英词典。 */
    static const QHash<QString, QString> &chineseDictionary();

    ~LanguageManager() override;

signals:
    void languageChanged();

private:
    explicit LanguageManager(QObject *parent = nullptr);

    class ZhTranslator;

    ZhTranslator *m_translator = nullptr;
    Language m_lang = English;

    void install();
};

#endif // LANGUAGE_H
