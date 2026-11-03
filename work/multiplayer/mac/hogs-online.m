#import <Cocoa/Cocoa.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <CommonCrypto/CommonDigest.h>

static NSString *Res(void);
static NSString *SHA(NSString *path) {
    NSFileHandle *h=[NSFileHandle fileHandleForReadingAtPath:path];if(!h)return nil;
    CC_SHA256_CTX c;CC_SHA256_Init(&c);NSData *d;
    while((d=[h readDataOfLength:1024*1024]).length)CC_SHA256_Update(&c,d.bytes,(CC_LONG)d.length);
    [h closeFile];unsigned char digest[CC_SHA256_DIGEST_LENGTH];CC_SHA256_Final(digest,&c);
    NSMutableString *s=[NSMutableString new];for(int i=0;i<CC_SHA256_DIGEST_LENGTH;i++)[s appendFormat:@"%02x",digest[i]];return s;
}


static NSString *State(void) { return [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/Hogs of War Online"]; }
static NSString *Prefix(void) { return [State() stringByAppendingPathComponent:@"prefix"]; }
static NSString *Game(void) { return [Prefix() stringByAppendingPathComponent:@"drive_c/HogsofWar"]; }
static NSString *Res(void) { return NSBundle.mainBundle.resourcePath; }
static BOOL OriginalArchives(NSString *source) {
    NSString *manifest=[NSString stringWithContentsOfFile:[Res() stringByAppendingPathComponent:@"original-archive-hashes.txt"] encoding:NSUTF8StringEncoding error:nil];
    if(!manifest)return NO;NSArray *lines=[manifest componentsSeparatedByCharactersInSet:NSCharacterSet.newlineCharacterSet];
    if(![lines.firstObject isEqualToString:@"HOGSORG1"])return NO;NSUInteger checked=0;
    for(NSUInteger i=1;i<lines.count;i++){
        NSString *line=lines[i];if(!line.length)continue;NSRange tab=[line rangeOfString:@"\t"];if(tab.location==NSNotFound)return NO;
        NSString *rel=[line substringToIndex:tab.location],*expected=[line substringFromIndex:tab.location+1];
        if(expected.length!=64||[rel hasPrefix:@"/"]||[rel containsString:@".."]||!([rel hasPrefix:@"Chars/"]||[rel hasPrefix:@"Maps/"]))return NO;
        if(![SHA([source stringByAppendingPathComponent:rel]) isEqualToString:expected])return NO;checked++;
    }
    return checked==140;
}
static BOOL PreserveUserData(NSString *oldGame, NSString *stage) {
    NSFileManager *fm=NSFileManager.defaultManager;if(![fm fileExistsAtPath:oldGame])return YES;
    NSMutableArray<NSString *> *toCopy=[NSMutableArray array];
    for(NSString *name in @[@"HogsFix.ini",@"dgVoodoo.conf",@"dxvk.conf"])if([fm fileExistsAtPath:[oldGame stringByAppendingPathComponent:name]])[toCopy addObject:name];
    NSDirectoryEnumerator *it=[fm enumeratorAtPath:oldGame];
    for(NSString *rel in it){NSString *base=rel.lastPathComponent.lowercaseString;if([base hasPrefix:@"savearmy"])[toCopy addObject:rel];}
    for(NSString *rel in toCopy){
        if([rel hasPrefix:@"/"]||[rel containsString:@".."]||[rel.pathComponents containsObject:@"."])return NO;
        NSString *from=[oldGame stringByAppendingPathComponent:rel],*to=[stage stringByAppendingPathComponent:rel];
        NSDictionary *attrs=[fm attributesOfItemAtPath:from error:nil];if(![attrs[NSFileType] isEqualToString:NSFileTypeRegular])continue;
        NSString *parent=to.stringByDeletingLastPathComponent;
        if(![fm createDirectoryAtPath:parent withIntermediateDirectories:YES attributes:nil error:nil])return NO;
        if([fm fileExistsAtPath:to]&&![fm removeItemAtPath:to error:nil])return NO;
        if(![fm copyItemAtPath:from toPath:to error:nil])return NO;
    }
    return YES;
}
static NSString *Wine(void) { return [NSBundle.mainBundle.bundlePath stringByAppendingPathComponent:@"Contents/SharedSupport/wine/bin/wine"]; }
static NSDictionary *Env(BOOL online) {
    NSMutableDictionary *e=[NSProcessInfo.processInfo.environment mutableCopy];
    for(NSString *key in @[@"WINEARCH",@"WINESERVER",@"WINELOADER",@"WINEDLLPATH"]) [e removeObjectForKey:key];
    e[@"WINEPREFIX"]=Prefix();
    e[@"DYLD_FALLBACK_LIBRARY_PATH"]=[NSBundle.mainBundle.bundlePath stringByAppendingPathComponent:@"Contents/Frameworks:/usr/lib"];
    e[@"WINEDLLOVERRIDES"]=online?@"ddraw,d3dimm=n;d3d11,dxgi,d3d10core=b;dinput=n,b;dplayx,dpwsockx=n;wsock32,mswsock=b;mscoree,mshtml=":@"ddraw,d3dimm=n;d3d11,dxgi,d3d10core=b;dinput=n,b;mscoree,mshtml=";
    e[@"WINEDEBUG"]=@"-all"; e[@"MVK_CONFIG_LOG_LEVEL"]=@"0"; e[@"DXVK_HUD"]=@"fps";
    e[@"DXVK_LOG_PATH"]=[State() stringByAppendingPathComponent:@"Logs"];
    e[@"DXVK_CONFIG_FILE"]=[Game() stringByAppendingPathComponent:@"dxvk.conf"];
    e[@"__COMPAT_LAYER"]=@"DWM8And16BitMitigation HIGHDPIAWARE";
    return e;
}
static BOOL Run(NSString *exe, NSArray *args, NSDictionary *env, NSString *cwd, NSString *log) {
    NSTask *t=[NSTask new]; t.executableURL=[NSURL fileURLWithPath:exe]; t.arguments=args;
    if(env)t.environment=env; if(cwd)t.currentDirectoryURL=[NSURL fileURLWithPath:cwd];
    NSFileHandle *h=nil;
    if(log){ [NSFileManager.defaultManager createFileAtPath:log contents:nil attributes:@{NSFilePosixPermissions:@0600}]; h=[NSFileHandle fileHandleForWritingAtPath:log]; t.standardOutput=h; t.standardError=h; }
    NSError *err=nil; if(![t launchAndReturnError:&err]){ [h closeFile]; return NO; }
    [t waitUntilExit]; [h closeFile]; return t.terminationStatus==0;
}
static BOOL GameRunning(void) {
    // Read-only guard also covers a separately installed, currently running copy.
    NSTask *t=[NSTask new]; t.executableURL=[NSURL fileURLWithPath:@"/usr/bin/pgrep"];
    t.arguments=@[@"-if",@"[w]arhogs_?\\.exe"]; t.standardOutput=NSFileHandle.fileHandleWithNullDevice;
    t.standardError=NSFileHandle.fileHandleWithNullDevice;
    if(![t launchAndReturnError:nil]) return YES;
    [t waitUntilExit]; return t.terminationStatus==0;
}
static void Alert(NSString *title,NSString *body) {
    NSAlert *a=[NSAlert new]; a.messageText=title; a.informativeText=body; [a addButtonWithTitle:@"Понятно"]; [a runModal];
}

@interface Setup : NSObject <NSApplicationDelegate>
@property NSWindow *window;
@property NSTextField *status;
@property NSButton *play;
@property NSButton *online;
@property BOOL busy;
@property BOOL networkPreparing;
@property NSString *pendingImport;
@property NSString *previewPath;
@property int guard;
@end
@implementation Setup
- (BOOL)ready {
    NSString *marker=[Game() stringByAppendingPathComponent:@".hogs-ready-v4"];
    NSString *expected=@"HOGSREADY4\nORIGINAL-HD24\n";
    NSString *actual=[NSString stringWithContentsOfFile:marker encoding:NSUTF8StringEncoding error:nil];
    if(![actual isEqualToString:expected] || ![NSFileManager.defaultManager fileExistsAtPath:[Game() stringByAppendingPathComponent:@"warhogs_.exe"]])return NO;
    if(![NSFileManager.defaultManager fileExistsAtPath:[Game() stringByAppendingPathComponent:@"TextureToolkit-x86.asi"]] ||
       ![NSFileManager.defaultManager fileExistsAtPath:[Game() stringByAppendingPathComponent:@"TextureToolkit.ini"]])return NO;
    NSArray *pages=[NSFileManager.defaultManager contentsOfDirectoryAtPath:[Game() stringByAppendingPathComponent:@"TT/inject"] error:nil];
    return pages.count==24;
}
- (BOOL)onlineReady {
    NSString *marker=[Prefix() stringByAppendingPathComponent:@".hogs-directplay-v1"];
    if(![NSFileManager.defaultManager fileExistsAtPath:marker])return NO;
    NSDictionary *expected=@{
        @"dplaysvr.exe":@"e3ee1e2d903237131ba42c770f07e7eceaa3574b4ce718062a4e3c1f9cdbf893",
        @"dplayx.dll":@"f50362966d69446f3f562946e77cb72ba8058abcddfadac4d2b0841e07434c3c",
        @"dpmodemx.dll":@"200753c96fd4ceac9b9bbed12a4b74d5c25df522cb32820dfc58d3d193be8eec",
        @"dpnet.dll":@"b3f41e537797abc414fda5136a864865c37e7324b55a9a5705c6761ff9a8e816",
        @"dpnhpast.dll":@"ab9743ce54837735f0e1cb8840ac58d578093078bc80e0dd91219dc37e5a703d",
        @"dpnhupnp.dll":@"59e74dd3de40e22aac95133cf746be32cb0de40e4fc801cf63be13d9e2ca8134",
        @"dpnsvr.exe":@"1ff263d2c1a97d422f94dd3d0872b1fa49de99b825ba64a5bf06f483a1849ac7",
        @"dpwsockx.dll":@"0725598871b2f1f65ccd0235683873346c8024a38dead045bce20526af65654d"
    };
    NSString *system=[Prefix() stringByAppendingPathComponent:@"drive_c/windows/syswow64"];
    for(NSString *name in expected)if(![SHA([system stringByAppendingPathComponent:name]) isEqualToString:expected[name]])return NO;
    return YES;
}
- (BOOL)application:(NSApplication *)sender openFile:(NSString *)filename {
    if(self.window)[self prepare:filename];else self.pendingImport=filename;return YES;
}
- (void)refresh {
    BOOL gameReady=[self ready];
    BOOL networkReady=[self onlineReady];
    self.status.stringValue=self.busy?(self.networkPreparing?@"Подготавливаем сетевой режим…":@"Подготавливаем игру…"):(gameReady?(networkReady?@"Игра подготовлена. Включён HD-профиль из 24 страниц.":@"Игра готова с HD-профилем из 24 страниц. Для сетевой игры один раз загрузятся компоненты Microsoft (около 109 МБ)."):@"Скачай Hogs of War из своего Steam или выбери папку с игрой.");
    self.play.enabled=gameReady&&!self.busy;
    self.online.title=networkReady?@"Играть по сети":@"Подготовить сетевой режим";
    self.online.enabled=gameReady&&!self.busy;
}
- (void)button:(NSString *)title action:(SEL)action y:(CGFloat)y {
    NSButton *b=[NSButton buttonWithTitle:title target:self action:action]; b.frame=NSMakeRect(36,y,548,36); [self.window.contentView addSubview:b];
}
- (void)mouseButton:(NSString *)title action:(SEL)action x:(CGFloat)x {
    NSButton *b=[NSButton buttonWithTitle:title target:self action:action]; b.frame=NSMakeRect(x,56,265,36); [self.window.contentView addSubview:b];
}
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    self.guard=-1;
    self.window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,620,520) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable backing:NSBackingStoreBuffered defer:NO];
    self.window.title=@"Hogs of War Online"; [self.window center];
    NSTextField *title=[NSTextField labelWithString:@"Hogs of War"]; title.font=[NSFont boldSystemFontOfSize:28]; title.frame=NSMakeRect(36,456,548,40); [self.window.contentView addSubview:title];
    self.status=[NSTextField wrappingLabelWithString:@""]; self.status.frame=NSMakeRect(36,400,548,48); [self.window.contentView addSubview:self.status];
    self.play=[NSButton buttonWithTitle:@"Играть" target:self action:@selector(playLocal:)]; self.play.frame=NSMakeRect(36,349,265,38); [self.window.contentView addSubview:self.play];
    self.online=[NSButton buttonWithTitle:@"Играть по сети" target:self action:@selector(playOnline:)]; self.online.frame=NSMakeRect(319,349,265,38); [self.window.contentView addSubview:self.online];
    [self button:@"Выбрать папку с игрой" action:@selector(choose:) y:294];
    [self button:@"Скачать из Steam" action:@selector(download:) y:247];
    [self button:@"Установить Tailscale" action:@selector(tailscale:) y:200];
    [self button:@"Как подключиться к другу" action:@selector(help:) y:153];
    [self button:@"Открыть журналы" action:@selector(logs:) y:103];
    [self button:@"Сведения и лицензии" action:@selector(notices:) y:10];
    [self refresh];
    if(self.previewPath){
        NSView *view=self.window.contentView;[view layoutSubtreeIfNeeded];
        NSBitmapImageRep *rep=[view bitmapImageRepForCachingDisplayInRect:view.bounds];
        [view cacheDisplayInRect:view.bounds toBitmapImageRep:rep];
        [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:self.previewPath atomically:YES];
        [NSApp terminate:nil];return;
    }
    [self.window makeKeyAndOrderFront:nil]; [NSApp activateIgnoringOtherApps:YES];
    if(self.pendingImport) [self prepare:self.pendingImport];
}
- (BOOL)lock {
    if(self.guard>=0)return NO;
    [NSFileManager.defaultManager createDirectoryAtPath:State() withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:@0700} error:nil];
    NSString *path=[State() stringByAppendingPathComponent:@"operation.lock"];
    self.guard=open(path.fileSystemRepresentation,O_RDWR|O_CREAT,0600);
    if(self.guard<0||flock(self.guard,LOCK_EX|LOCK_NB)) { if(self.guard>=0)close(self.guard); self.guard=-1; return NO; }
    return YES;
}
- (void)unlock { if(self.guard>=0){flock(self.guard,LOCK_UN);close(self.guard);self.guard=-1;} }
- (void)choose:(id)sender {
    if(self.busy)return; NSOpenPanel *p=NSOpenPanel.openPanel; p.canChooseDirectories=YES; p.canChooseFiles=NO;
    p.message=@"Выбери папку, в которой лежит warhogs_.exe.";
    if([p runModal]==NSModalResponseOK)[self prepare:p.URL.path];
}
- (void)prepare:(NSString *)source {
    if(self.busy)return;
    if(GameRunning()){Alert(@"Сначала закрой игру",@"Подготовку можно продолжить после выхода из Hogs of War.");return;}
    if([self ready]){Alert(@"Игра уже подготовлена",@"Повторная установка не требуется. Сохранения находятся в этой копии игры.");return;}
    if(![NSFileManager.defaultManager fileExistsAtPath:[source stringByAppendingPathComponent:@"warhogs_.exe"]]){Alert(@"Не найдены файлы игры",@"Выбери папку с warhogs_.exe или скачай игру из Steam.");return;}
    for(NSString *asset in @[@"Chars/british.mad",@"Chars/british.mtd",@"Maps/camp.mad",@"Maps/camp.mtd",@"Data/_d3d.dll"]){
        if(![NSFileManager.defaultManager fileExistsAtPath:[source stringByAppendingPathComponent:asset]]){Alert(@"Не хватает файлов игры",@"Повтори скачивание в Steam с проверкой файлов, затем выбери полную папку Hogs of War.");return;}
    }
    if(![SHA([source stringByAppendingPathComponent:@"warhogs_.exe"]) isEqualToString:@"32eb8c28b383d532091dcae2057d98593be4dc064f2117212a65ced41509e246"]){Alert(@"Другая версия игры",@"Эта сборка подготовлена для версии Hogs of War из Steam. Скачай её через «Скачать из Steam».");return;}
    if(!OriginalArchives(source)){Alert(@"В этой папке изменены игровые архивы",@"Эта подготовка использует исходные текстуры. В Steam выбери «Свойства → Установленные файлы → Проверить целостность» или укажи чистую копию игры. Ничего не скопировано.");return;}
    if(![self lock]){Alert(@"Подготовка уже идёт",@"Дождись её завершения в другом окне.");return;}
    self.busy=YES;[self refresh];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
        NSFileManager *fm=NSFileManager.defaultManager;
        NSString *logs=[State() stringByAppendingPathComponent:@"Logs"];
        [fm createDirectoryAtPath:logs withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:@0700} error:nil];
        BOOL ok=Run(@"/usr/bin/arch",@[@"-x86_64",@"/usr/bin/true"],nil,nil,nil);
        if(ok)ok=Run(Wine(),@[@"wineboot",@"--init"],Env(NO),State(),[logs stringByAppendingPathComponent:@"setup.log"]);
        NSString *server=[Wine().stringByDeletingLastPathComponent stringByAppendingPathComponent:@"wineserver"];
        if(ok)ok=Run(server,@[@"-w"],Env(NO),State(),[logs stringByAppendingPathComponent:@"setup-wait.log"]);
        NSString *stage=[Prefix() stringByAppendingPathComponent:@"drive_c/HogsofWar.installing"];
        if(ok){[fm removeItemAtPath:stage error:nil];ok=Run(@"/usr/bin/ditto",@[source,stage],nil,nil,[logs stringByAppendingPathComponent:@"copy.log"]);}
        NSDictionary *manifest=[NSDictionary dictionaryWithContentsOfFile:[Res() stringByAppendingPathComponent:@"payload.plist"]];
        if(manifest.count!=36)ok=NO;
        for(NSString *name in manifest){
            if(!ok)break;
            NSString *from=[[Res() stringByAppendingPathComponent:@"payload"] stringByAppendingPathComponent:name];
            NSString *to=[stage stringByAppendingPathComponent:name];
            if(![SHA(from) isEqualToString:manifest[name]]){ok=NO;break;}
            [fm createDirectoryAtPath:to.stringByDeletingLastPathComponent withIntermediateDirectories:YES attributes:nil error:nil];
            [fm removeItemAtPath:to error:nil];ok=[fm copyItemAtPath:from toPath:to error:nil];
        }
        NSString *saveSource=Game();
        if(ok)ok=PreserveUserData(saveSource,stage);
        for(NSString *name in @[@"d3d11.dll",@"dxgi.dll",@"d3d10core.dll"]){
            if(!ok)break;
            NSString *to=[[Prefix() stringByAppendingPathComponent:@"drive_c/windows/syswow64"] stringByAppendingPathComponent:name];
            [fm removeItemAtPath:to error:nil];ok=[fm copyItemAtPath:[stage stringByAppendingPathComponent:name] toPath:to error:nil];
        }
        if(ok){
            // Commit the ready marker with the game directory in one rename.
            NSString *ready=@"HOGSREADY4\nORIGINAL-HD24\n";
            ok=[ready writeToFile:[stage stringByAppendingPathComponent:@".hogs-ready-v4"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
            NSString *previous=[Prefix() stringByAppendingPathComponent:[@"drive_c/HogsofWar.previous-" stringByAppendingString:NSUUID.UUID.UUIDString]];
            BOOL movedPrevious=NO;
            if(ok&&[fm fileExistsAtPath:Game()]){ok=[fm moveItemAtPath:Game() toPath:previous error:nil];movedPrevious=ok;}
            if(ok){ok=[fm moveItemAtPath:stage toPath:Game() error:nil];if(!ok&&movedPrevious)[fm moveItemAtPath:previous toPath:Game() error:nil];}
        }
        if(!ok)[fm removeItemAtPath:stage error:nil];
        dispatch_async(dispatch_get_main_queue(),^{self.busy=NO;[self unlock];[self refresh];
            if(ok)Alert(@"Игра подготовлена",@"В отдельном префиксе создана копия с HD-профилем из 24 страниц. Игровые архивы и модели сохранены. Существующая копия сохранена рядом. Для первого запуска выбери dgVoodoo DirectX Wrapper и разрешение 640×480, затем нажми Start.");
            else Alert(@"Подготовка не завершена",@"Если Rosetta ещё не установлена, нажми «Скачать из Steam»: в окне загрузки можно установить её. Затем повтори подготовку. Подробности находятся в журналах.");
        });
    });
}
- (void)download:(id)sender {
    if(self.busy)return;
    if([self ready]){Alert(@"Игра уже подготовлена",@"Повторное скачивание не требуется.");return;}
    if(GameRunning()){Alert(@"Сначала закрой игру",@"Скачивание и подготовка доступны после выхода из Hogs of War.");return;}
    [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:[Res() stringByAppendingPathComponent:@"download-steam.command"]]];
}
- (void)playLocal:(id)sender {[self launch:NO];}
- (void)playOnline:(id)sender {
    if([self onlineReady]){[self launch:YES];return;}
    [self prepareDirectPlay];
}
- (void)prepareDirectPlay {
    if(self.busy)return;
    if(GameRunning()){Alert(@"Сначала закрой игру",@"Сетевой режим можно подготовить после выхода из Hogs of War.");return;}
    if(![self lock]){Alert(@"Подготовка уже идёт",@"Дождись её завершения в другом окне.");return;}
    self.busy=YES;self.networkPreparing=YES;[self refresh];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
        NSFileManager *fm=NSFileManager.defaultManager;
        NSString *logs=[State() stringByAppendingPathComponent:@"Logs"];
        [fm createDirectoryAtPath:logs withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:@0700} error:nil];
        NSString *helper=[Res() stringByAppendingPathComponent:@"prepare-directplay.command"];
        NSMutableDictionary *env=[Env(NO) mutableCopy];[env removeObjectForKey:@"HOGS_DIRECTPLAY_ARCHIVE_URL"];env[@"HOGS_APP_BUNDLE"]=NSBundle.mainBundle.bundlePath;
        BOOL ok=Run(helper,@[@"install"],env,State(),[logs stringByAppendingPathComponent:@"directplay-install.log"]);
        ok=ok&&[self onlineReady];
        dispatch_async(dispatch_get_main_queue(),^{self.busy=NO;self.networkPreparing=NO;[self unlock];[self refresh];
            if(ok)Alert(@"Сетевой режим подготовлен",@"Теперь можно нажать «Играть по сети». Матч между Mac и Windows ещё не проверен.");
            else Alert(@"Не удалось подготовить сеть",@"Игра не запускалась. Изменения компонентов отменены; подробности — в журнале «directplay-install.log».");
        });
    });
}
- (void)launch:(BOOL)online {
    if(![self ready]||self.busy)return;
    if(GameRunning()){Alert(@"Игра уже запущена",@"Вернись в открытое окно Hogs of War.");return;}
    if(![self lock])return;
    self.busy=YES;[self refresh];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
        BOOL ok=Run(Wine(),@[@"C:\\HogsofWar\\warhogs_.exe"],Env(online),Game(),[[State() stringByAppendingPathComponent:@"Logs"] stringByAppendingPathComponent:online?@"online.log":@"game.log"]);
        dispatch_async(dispatch_get_main_queue(),^{self.busy=NO;[self unlock];[self refresh];if(!ok)Alert(@"Игра завершилась с ошибкой",@"Открой журналы запуска, чтобы передать их для проверки.");});
    });
}
- (void)tailscale:(id)sender {
    // Installation/account approval remains in the vendor's official UI.
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"https://tailscale.com/download/mac"]];
}
- (void)help:(id)sender {[[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:[Res() stringByAppendingPathComponent:@"Как играть по сети.txt"]]];}
- (void)notices:(id)sender {[[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:[Res() stringByAppendingPathComponent:@"Third Party Notices"]]];}
- (void)logs:(id)sender {[[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:[State() stringByAppendingPathComponent:@"Logs"]]];}
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    if(self.busy){Alert(@"Дождись завершения",@"Во время подготовки дождись её окончания. Если открыта игра, сначала выйди из неё через игровое меню.");return NSTerminateCancel;} return NSTerminateNow;
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {return !self.busy;}
@end

int main(int argc,char **argv){@autoreleasepool{
    if(argc==2&&!strcmp(argv[1],"--check")){
        NSArray *names=@[@"payload.plist",@"payload/TextureToolkit-x86.asi",@"payload/TextureToolkit.ini",@"original-archive-hashes.txt",@"download-steam.command",@"prepare-directplay.command",@"Как играть по сети.txt",@"Third Party Notices/Microsoft-DirectX-EULA-ru.txt"];
        BOOL ok=[NSFileManager.defaultManager isExecutableFileAtPath:Wine()];
        for(NSString *n in names)ok=ok&&[NSFileManager.defaultManager fileExistsAtPath:[Res() stringByAppendingPathComponent:n]];
        NSDictionary *manifest=[NSDictionary dictionaryWithContentsOfFile:[Res() stringByAppendingPathComponent:@"payload.plist"]];
        ok=ok&&manifest.count==36;
        for(NSString *n in manifest)ok=ok&&[SHA([[Res() stringByAppendingPathComponent:@"payload"] stringByAppendingPathComponent:n]) isEqualToString:manifest[n]];
        puts(ok?"{\"bundle_check\":true,\"wine_started\":false}":"{\"bundle_check\":false,\"wine_started\":false}");return ok?0:2;
    }
    [NSApplication sharedApplication];Setup *app=[Setup new];
    if(argc==3&&!strcmp(argv[1],"--import"))app.pendingImport=[NSString stringWithUTF8String:argv[2]];
    if(argc==3&&!strcmp(argv[1],"--snapshot"))app.previewPath=[NSString stringWithUTF8String:argv[2]];
    NSApp.delegate=app;[NSApp setActivationPolicy:app.previewPath?NSApplicationActivationPolicyAccessory:NSApplicationActivationPolicyRegular];[NSApp run];
    return 0;
}}
