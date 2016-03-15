/* vokoscreen - A desktop recorder
 * Copyright (C) 2011-2014 Volker Kohaupt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301 USA 
 */

#include "screencast.h"  
#include "QvkFormatsAndCodecs.h"
#include "QvkCountdown.h"
#include "QvkPulse.h"

using namespace std;

#ifdef QT5
  #include "QvkLogListWidget.h"
  #include "log/QvkLog.h"
  #include <QPointer>

  QPointer<QvkLog> myLog;

  void myMessageOutput( QtMsgType type, const QMessageLogContext &context, const QString &msg )
  {
    myLog->outputMessage( type, context, msg );
  }
#endif


screencast::screencast()
{
    vkSettings.readAll();
    
    myUi.setupUi( this );
    myUi.ListWidgetLogVokoscreen->setVisible( false );

#ifdef QT5
    // http://qt-project.org/doc/qt-5/qtglobal.html#qInstallMessageHandler
    myLog = new QvkLog();
    qInstallMessageHandler( myMessageOutput );
    connect( myLog, SIGNAL( newLogText( QString ) ), this, SLOT( addLogVokoscreen( QString ) ) );
#endif
    
    oldMainWindowHeight = height();

    screencast::setWindowTitle( vkSettings.getProgName() + " " + vkSettings.getVersion() );

    QIcon icon;
    icon.addFile( QString::fromUtf8( ":/pictures/vokoscreen.png" ), QSize(), QIcon::Normal, QIcon::Off );
    screencast::setWindowIcon( icon );    

    qDebug() << "[vokoscreen]" << "Version:" << vkSettings.getVersion();
    qDebug() << "[vokoscreen]" << "Qt version: " << qVersion();
    qDebug() << "[vokoscreen]" << "Operating system:" << getOsRelease();
    qDebug() << "[vokoscreen]" << "Desktop:" << qgetenv( "XDG_CURRENT_DESKTOP" );
    QvkAlsaDevice inBox;
    qDebug() << "[vokoscreen]" << "asoundlib version:" << inBox.getAlsaVersion();
    qDebug() << "[vokoscreen] current icon-theme:" << QIcon::themeName();
    qDebug( " " );

    searchExternalPrograms();

    pause = false;
    firststartWininfo = false;
    

    // Tab 1 Screen options ***************************************************
    myUi.tabWidget->setTabIcon( 0, QIcon::fromTheme( "video-display", QIcon( ":/pictures/monitor.png" ) ) );
    myUi.updateButton->setIcon( QIcon( ":/pictures/system-software-update.png" ) );

    connect( myUi.FullScreenRadioButton, SIGNAL( clicked() ), SLOT( clickedScreenSize() ) );
    connect( myUi.WindowRadioButton, SIGNAL( clicked() ), SLOT( clickedScreenSize() ) );
    connect( myUi.AreaRadioButton, SIGNAL( clicked() ), SLOT( clickedScreenSize() ) );
    
    QDesktopWidget *desk = QApplication::desktop();
    myScreenCountChanged( desk->screenCount() );
    connect( desk, SIGNAL( screenCountChanged(int) ), SLOT( myScreenCountChanged(int) ) );
    connect( desk, SIGNAL( resized(int) ), SLOT( myScreenCountChanged(int) ) );

    // Give the Display :0 or :1 etc.
    qDebug() << "[vokoscreen]" << "---Begin Environment---";
    DISPLAY = qgetenv( "DISPLAY" );
    qDebug() << "[vokoscreen] runs on DISPLAY" << DISPLAY;
    qDebug() << "[vokoscreen]" << "---End Environment---";
    qDebug( " " );

    connect( myUi.MagnifierCheckBox, SIGNAL( clicked() ), SLOT( showMagnifier() ) );
    magnifier = new QvkMagnifier();
    magnifier->close();
    connect( magnifier, SIGNAL( closeMagnifier() ), SLOT( uncheckMagnifier() ) );
    connect( myUi.MagnifierDialogPushButton, SIGNAL( clicked() ), magnifier,  SLOT( showDialogMagnifier() ) );

    QvkShowkeyController *showkeyController = new QvkShowkeyController( myUi.ShowkeyQCheckBox );
    (void)showkeyController;
    
    // Begin showclick
    QColor color = Qt::red;
    bool radiant = false;
    double opacity = 0.5;
    ShowClickDialog = new QvkShowClickDialog( color, radiant, opacity );
    connect( myUi.pointerDialogPushButton, SIGNAL( clicked() ), ShowClickDialog, SLOT( show() ) );
  
    animateControl = new QvkAnimateControl( (double) ShowClickDialog->myUiDialog.horizontalSliderShowtime->value()/10,
					    ShowClickDialog->myUiDialog.horizontalSliderCircle->value(),
					    ShowClickDialog->myUiDialog.checkBoxRadiant->checkState(),
					    (double) ShowClickDialog->myUiDialog.horizontalSliderOpacity->value()/100,
					    color );

    connect( myUi.pointerQCheckBox, SIGNAL( clicked( bool ) ), animateControl, SLOT( pointerOnOff( bool ) ) );
  
    connect( ShowClickDialog, SIGNAL( newCircleWidgetValue( int, QColor ) ), animateControl, SLOT( setDiameterColor( int, QColor ) ) );
    connect( ShowClickDialog, SIGNAL( newShowtime( double ) ), animateControl, SLOT( setShowTime( double ) ) );
    connect( ShowClickDialog, SIGNAL( newOpacity( double ) ), animateControl, SLOT( setOpacity( double ) ) );
    connect( ShowClickDialog, SIGNAL( newRadiant( bool ) ), animateControl, SLOT( setRadiant( bool ) ) );
    // End showclick
    
    
    // Tab 2 Audio options ****************************************
    myUi.tabWidget->setTabIcon( 1, QIcon::fromTheme( "audio-input-microphone", QIcon( ":/pictures/micro.png" ) ) );
    connect( myUi.AudioOnOffCheckbox,  SIGNAL( stateChanged( int ) ), SLOT( stateChangedAudio( int ) ) );
    connect( myUi.AudioOnOffCheckbox,  SIGNAL( stateChanged( int ) ), SLOT( AudioOff( int ) ) );
    connect( myUi.AlsaRadioButton,  SIGNAL( clicked( bool )  ), SLOT( clickedAudioAlsa( bool ) ) );
    connect( myUi.PulseDeviceRadioButton,  SIGNAL( clicked( bool )  ), SLOT( clickedAudioPulse( bool ) ) );

    
    // Tab 3 Video options **************************************************
    myUi.tabWidget->setTabIcon( 2, QIcon::fromTheme( "applications-multimedia", QIcon( ":/pictures/videooptionen.png" ) ) );
    
    connect( myUi.VideocodecComboBox, SIGNAL( currentIndexChanged( int ) ), SLOT( currentIndexChangedCodec( int ) ) );
    connect( myUi.VideoContainerComboBox, SIGNAL( currentIndexChanged( int ) ), SLOT( currentIndexChangedFormat( int ) ) );

    myUi.FrameStandardButton->setIcon ( QIcon::fromTheme( "edit-undo", QIcon( ":/pictures/undo.png" ) ) );
    myUi.FrameStandardButton->setToolTip( tr( "Default" ) );
    connect( myUi.FrameStandardButton, SIGNAL( clicked() ), SLOT( setFrameStandardSpinbox() ) );

    connect( myUi.FrameSpinBox, SIGNAL( valueChanged( int ) ), SLOT( valueChangedFrames( int ) ) );// Zeigt Änderungen in Statusbar an

    myUi.VideocodecStandardButton->setIcon ( QIcon::fromTheme( "edit-undo", QIcon( ":/pictures/undo.png" ) ) );
    myUi.VideocodecStandardButton->setToolTip( tr( "Default" ) );
    connect( myUi.VideocodecStandardButton, SIGNAL( clicked() ), SLOT( setVideocodecStandardComboBox() ) );

    myUi.AudiocodecStandardButton->setIcon ( QIcon::fromTheme( "edit-undo", QIcon( ":/pictures/undo.png" ) ) );
    myUi.AudiocodecStandardButton->setToolTip( tr( "Default" ) );
    connect( myUi.AudiocodecStandardButton, SIGNAL( clicked() ), SLOT( setAudiocodecStandardComboBox() ) );


    // Tab 4 Miscellaneous options **************************************************
    myUi.tabWidget->setTabIcon( 3, QIcon::fromTheme( "preferences-system", QIcon( ":/pictures/tools.png" ) ) );

    connect( myUi.SaveVideoPathPushButton, SIGNAL(clicked() ), SLOT( saveVideoPath() ) );

    connect( myUi.RecorderLineEdit, SIGNAL( textChanged( QString ) ), SLOT( recorderLineEditTextChanged( QString ) ) );
    myUi.RecorderLineEdit->setText( getFileWithPath( vkSettings.getRecorder() ) );

    connect( myUi.selectRecorderPushButton, SIGNAL(clicked() ), SLOT( selectRecorder() ) );
    
    
    // Tab 5 Webcam *******************************************************
    myUi.tabWidget->setTabIcon( 4, QIcon::fromTheme( "camera-web", QIcon( ":/pictures/webcam.png" ) ) );
    myUi.webcamCheckBox->setToolTip( "CTRL+SHIFT+F8" );
    myUi.webcamComboBox->setToolTip( tr ( "Select webcam" ) );
    myUi.mirrorCheckBox->setText( tr( "Mirrored" ) );
    myUi.rotateDial->setWrapping ( true );
    webcamController = new QvkWebcamController( myUi.webcamCheckBox, myUi.webcamComboBox, myUi.mirrorCheckBox, 
						myUi.dialFrame, myUi.rotateDial, myUi.radioButtonTopMiddle,
						myUi.radioButtonRightMiddle, myUi.radioButtonBottomMiddle, myUi.radioButtonLeftMiddle );
    (void)webcamController;

    // Tab 6 About *********************************************************
    // http://doc.qt.io/qt-5/qdesktopservices.html#openUrl
    
    myUi.tabWidget->setTabIcon( 5, QIcon::fromTheme( "dialog-information", QIcon( ":/pictures/about.png" ) ) );
    myUi.labelOpensuseBetaUrl->setOpenExternalLinks( true );
    myUi.labelOpensuseBetaUrl->setText( "<a href='http://linuxecke.volkoh.de/vokoscreen/vokoscreen.html'>" + tr( "Developer Homepage" ) + "</a>" );
    
    QString homepage = "<a href='http://www.kohaupt-online.de/hp'>" + tr( "Homepage" ) + "</a>";
    myUi.labelWebSite->setOpenExternalLinks( true );
    myUi.labelWebSite->setText( homepage );
    
    QString email = "<a href='mailto:tux@kohaupt-online.de?subject=vokoscreen ";
    email = email.append( vkSettings.getVersion() ).append( "'" ).append( ">" + tr( "Support" ) + "</a>" );
    myUi.labelMail->setOpenExternalLinks( true );
    myUi.labelMail->setText( email );

    QString emaildeveloper = "<a href ='mailto:vkohaupt@freenet.de?subject=vokoscreen ";
    emaildeveloper = emaildeveloper.append( vkSettings.getVersion() ).append( "'" ).append( ">" + tr( "Developer" ) + "</a>" );
    myUi.labelDeveLoperMail->setOpenExternalLinks( true );
    myUi.labelDeveLoperMail->setText( emaildeveloper );
    
    QString Sourcecode = "<a href='https://github.com/vkohaupt/vokoscreen'>" + tr( "Sourcecode" ) + "</a>";
    myUi.labelSourcecodeUrl->setOpenExternalLinks( true );
    myUi.labelSourcecodeUrl->setText( Sourcecode );
       
    myUi.labelLanguageUrl->setOpenExternalLinks( true );
    myUi.labelLanguageUrl->setText( "<a href='https://www.transifex.com/projects/p/vokoscreen/'>" + tr( "Translations" ) + "</a>" );

    myUi.labelDonateUrl->setOpenExternalLinks( true );
    myUi.labelDonateUrl->setText( "<a href='http://www.kohaupt-online.de/hp/spende.html'>" + tr( "Donate" ) + "</a>" );
    
    QIcon creditsIcon;
    creditsIcon.addFile( ":/pictures/community.png", QSize(), QIcon::Normal, QIcon::On );
    myUi.creditsQPushButton->setIcon( creditsIcon );
    connect( myUi.creditsQPushButton, SIGNAL( clicked() ), SLOT( showCredits() ) );

    
    
    // Start Stop Pause etc. Buttons
    myUi.recordButton->setToolTip( "CTRL+SHIFT+F10" );
    connect( myUi.recordButton, SIGNAL( clicked() ), SLOT( preRecord() ) );
    
    myUi.StopButton->setToolTip( "CTRL+SHIFT+F11" );
    myUi.StopButton->setEnabled( false );
    connect( myUi.StopButton, SIGNAL( clicked() ), SLOT( Stop() ) );

    myUi.PauseButton->setToolTip( "CTRL+SHIFT+F12" );
    myUi.PauseButton->setCheckable( true );
    myUi.PauseButton->setEnabled( false );
    connect( myUi.PauseButton, SIGNAL( clicked() ), SLOT( Pause() ) );

    myUi.PlayButton->setToolTip( tr( "Play last Video" ) );
    connect( myUi.PlayButton, SIGNAL( clicked() ), SLOT( play() ) );
    
    myUi.sendPushButton->setToolTip( tr( "Send Video" ) );
    connect( myUi.sendPushButton, SIGNAL( clicked() ), SLOT( send() ) );
    if ( searchProgramm( "xdg-email" ) )
      myUi.sendPushButton->setEnabled( true );
    else
      myUi.sendPushButton->setEnabled( false );
    
    myUi.LogPushButton->setIcon ( QIcon::fromTheme( "dialog-information", QIcon( ":/pictures/about.png" ) ) );
    connect( myUi.LogPushButton, SIGNAL( clicked() ), this, SLOT( VisibleHideKonsole() ) );
    
    // StatusBar
    statusBarLabelTime = new QLabel();
    statusBarLabelTime->setText( "00:00:00" );
    //statusBarLabelTime->setFrameStyle( QFrame::StyledPanel | QFrame::Sunken );
    statusBarLabelTime->setToolTip( tr ( "Recording time" ) );

    statusBarLabelFps = new QLabel();
    statusBarLabelFps->setText( "0" );
    //statusBarLabelFps->setFrameStyle( QFrame::StyledPanel | QFrame::Sunken );
    statusBarLabelFps->setToolTip( tr( "Actual frames per second" ) );

    statusBarLabelSize = new QLabel();
    statusBarLabelSize->setText( "0" );
    //statusBarLabelSize->setFrameStyle( QFrame::StyledPanel | QFrame::Sunken );
    statusBarLabelSize->setToolTip( tr( "Size in KB" ) );
    
    statusbarLabelScreenSize = new QLabel();
    //statusbarLabelScreenSize->setFrameStyle( QFrame::StyledPanel | QFrame::Sunken );
    statusbarLabelScreenSize->setToolTip( tr( "Recording screensize" ) );

    statusBarLabelCodec = new QLabel();
    statusBarLabelCodec->setText( myUi.VideocodecComboBox->currentText() );
    //statusBarLabelCodec->setFrameStyle( QFrame::StyledPanel | QFrame::Sunken );
    statusBarLabelCodec->setToolTip( tr( "Codec" ) );
    
    statusBarLabelFormat = new QLabel();
    statusBarLabelFormat->setText( myUi.VideoContainerComboBox->currentText() );
    //statusBarLabelFormat->setFrameStyle( QFrame::StyledPanel | QFrame::Sunken );
    statusBarLabelFormat->setToolTip( tr( "Format" ) );

    statusBarLabelAudio = new QLabel();
    //statusBarLabelAudio->setFrameStyle( QFrame::StyledPanel | QFrame::Sunken );
    statusBarLabelAudio->setToolTip( tr( "Audio" ) );
    
    statusBarLabelFpsSettings = new QLabel();
    //statusBarLabelFpsSettings->setFrameStyle( QFrame::StyledPanel | QFrame::Sunken );
    statusBarLabelFpsSettings->setToolTip( tr( "Settings fps" ) );

    QLabel * LabelTemp = new QLabel();
    myUi.statusBar->addWidget( LabelTemp, 0 );
    
    myUi.statusBar->addWidget( statusBarLabelTime, 2 );
    myUi.statusBar->addWidget( statusBarLabelFps, 2 );
    myUi.statusBar->addWidget( statusBarLabelSize, 2 );
    myUi.statusBar->addWidget( statusbarLabelScreenSize, 2 );
    myUi.statusBar->addWidget( statusBarLabelCodec, 2 );
    myUi.statusBar->addWidget( statusBarLabelFormat, 2 );
    myUi.statusBar->addWidget( statusBarLabelAudio, 2 );
    myUi.statusBar->addWidget( statusBarLabelFpsSettings, 2 );
    
    searchVideoPlayer();
    searchGIFPlayer();
    
    // Read Settings
    if ( vkSettings.getVideoPath() > "" )
       myUi.SaveVideoPathLineEdit->setText( vkSettings.getVideoPath() );
    else
       PathMoviesLocation();

    SystemCall = new QProcess( this );
    
    connect( myUi.AudioOnOffCheckbox,     SIGNAL( clicked() ), SLOT( AudioOnOff() ) );
    connect( myUi.AlsaRadioButton,        SIGNAL( clicked() ), SLOT( AudioOnOff() ) );
    connect( myUi.PulseDeviceRadioButton, SIGNAL( clicked() ), SLOT( AudioOnOff() ) );

    connect( SystemCall, SIGNAL( stateChanged ( QProcess::ProcessState) ),this, SLOT( stateChanged( QProcess::ProcessState) ) );
    connect( SystemCall, SIGNAL( error( QProcess::ProcessError) ),        this, SLOT( error( QProcess::ProcessError) ) );
    connect( SystemCall, SIGNAL( readyReadStandardError() ),              this, SLOT( readyReadStandardError() ) );

    windowMoveTimer = new QTimer( this );
    connect( windowMoveTimer, SIGNAL( timeout() ), this, SLOT( windowMove() ) );

    // Area ein-ausblenden wenn Radiobutton immer wieder angecklickt wird
    connect( myUi.AreaRadioButton,       SIGNAL( clicked() ), SLOT( AreaOnOff() ) );
    connect( myUi.FullScreenRadioButton, SIGNAL( clicked() ), SLOT( AreaOnOff() ) );
    connect( myUi.WindowRadioButton,     SIGNAL( clicked() ), SLOT( AreaOnOff() ) );
    myregionselection = new regionselection();
    
    // Clean vokoscreen temp
    QDir dir( PathTempLocation() );
    QStringList stringList = dir.entryList( QDir::Files, QDir::Time | QDir::Reversed );
    for ( int i = 0; i < stringList.size(); ++i )
      dir.remove( PathTempLocation().append( QDir::separator() ).append( stringList.at( i ) ) );
    
   AudioOnOff();
   QvkPulse::pulseUnloadModule();
    
   QAction *vokoscreenAction = new QAction( this );
   vokoscreenAction->setIcon( QIcon( ":/pictures/systray.png" ) );
   vokoscreenAction->setText( "vokoscreen" );
   vokoscreenAction->setEnabled( true );
   
   startAction = new QAction( this );
   startAction->setIcon( QIcon::fromTheme( "media-playback-start", QIcon( ":/pictures/start.png" ) ) );
   startAction->setText( tr( "Start" ) );
   startAction->setData( "Start" );
   
   stopAction = new QAction( this );
   stopAction->setIcon( QIcon::fromTheme( "media-playback-stop" , QIcon( ":/pictures/stop.png" ) ) );
   stopAction->setText( tr( "Stop" ) );
   stopAction->setData( "Stop" );
   stopAction->setEnabled( false );
   
   pauseAction = new QAction( this );
   pauseAction->setIcon( QIcon::fromTheme( "media-playback-pause", QIcon( ":/pictures/pause.png" ) ) );
   pauseAction->setText( tr( "Pause" ) );
   pauseAction->setData( "Pause" );
   pauseAction->setEnabled( false );
   
   goAction = new QAction( this );
   goAction->setIcon( QIcon::fromTheme( "media-skip-forward", QIcon( ":/pictures/go.png" ) ) );
   goAction->setText( tr( "Go" ) );
   goAction->setData( "Go" );
   goAction->setEnabled( false );
   
   hideAction = new QAction( this );
   hideAction->setIcon( QIcon::fromTheme( "video-display", QIcon( ":/pictures/systray.png" ) ) );
   hideAction->setText( tr( "Hide window" ) );
   hideAction->setData( "Hide" );
   
   exitAction = new QAction( this );
   exitAction->setIcon( QIcon::fromTheme( "application-exit"  , QIcon( ":/pictures/systray-exit.png" ) ) );
   exitAction->setText( tr( "Exit" ) );
   exitAction->setData( "Exit" );
   
   SystemTrayMenu = new QMenu();
   SystemTrayMenu->addAction( vokoscreenAction );
   SystemTrayMenu->addSeparator();
   SystemTrayMenu->addAction( startAction );
   SystemTrayMenu->addAction( stopAction );
   SystemTrayMenu->addAction( pauseAction );
   SystemTrayMenu->addAction( goAction );
   SystemTrayMenu->addSeparator();
   SystemTrayMenu->addAction( hideAction );
   SystemTrayMenu->addSeparator();
   SystemTrayMenu->addAction( exitAction );
   connect( SystemTrayMenu, SIGNAL( triggered( QAction * ) ), this, SLOT( SystemTrayKontextMenue( QAction * ) ) );
   
   SystemTrayIcon = new QSystemTrayIcon( QIcon( ":/pictures/systray.png" ) );
   SystemTrayIcon->setContextMenu ( SystemTrayMenu );
   SystemTrayIcon->setToolTip( "vokoscreen" );
   SystemTrayIcon->show();
   myUi.SystrayCheckBox->setCheckState( Qt::CheckState( vkSettings.getSystray() ) );
    
   shortcutWebcam = new QxtGlobalShortcut( this );
   connect( shortcutWebcam, SIGNAL( activated() ), myUi.webcamCheckBox, SLOT( click() ) );
   shortcutWebcam->setShortcut( QKeySequence( "Ctrl+Shift+F8" ) );
   
   shortcutMagnifier = new QxtGlobalShortcut( this );
   connect( shortcutMagnifier, SIGNAL( activated() ), myUi.MagnifierCheckBox, SLOT( click() ) );
   shortcutMagnifier->setShortcut( QKeySequence( "Ctrl+Shift+F9" ) );

   shortcutStart = new QxtGlobalShortcut( this );
   connect( shortcutStart, SIGNAL( activated() ), this, SLOT( preRecord() ) );
   shortcutStart->setShortcut( QKeySequence( "Ctrl+Shift+F10" ) );
   
   shortcutStop = new QxtGlobalShortcut( this );
   connect( shortcutStop, SIGNAL( activated() ), this, SLOT( Stop() ) );
   shortcutStop->setShortcut( QKeySequence( "Ctrl+Shift+F11" ) );
   shortcutStop->setEnabled( false );
   
   shortcutPause = new QxtGlobalShortcut( this );
   connect( shortcutPause, SIGNAL( activated() ), this, SLOT( ShortcutPause() ) );
   shortcutPause->setShortcut( QKeySequence( "Ctrl+Shift+F12" ) );
   
   
   QvkAlsaWatcher * myAlsaWatcher = new QvkAlsaWatcher();
   connect( myAlsaWatcher, SIGNAL( changed( QStringList ) ), this, SLOT( AlsaWatcherEvent( QStringList ) ) );
    
   QFileSystemWatcher * VideoFileSystemWatcher = new QFileSystemWatcher();
   VideoFileSystemWatcher->addPath( myUi.SaveVideoPathLineEdit->displayText() );
   connect( VideoFileSystemWatcher, SIGNAL( directoryChanged( const QString& ) ), this, SLOT( myVideoFileSystemWatcher( const QString ) ) );
   myVideoFileSystemWatcher( "" );
    
   qDebug() << "[vokoscreen] ---Begin search video codec---";
   QvkFormatsAndCodecs *formatsAndCodecs = new QvkFormatsAndCodecs( "/usr/bin/ffmpeg" );// RecorderLineEdit->displayText() );*************************************************************************
   QStringList videoCodecList;
   bool experimental = false;
   videoCodecList << "libx264" << "libx265" << "mpeg4" << "huffyuv" << "gif";
   for ( int i = 0; i < videoCodecList.count(); i++ )
   {
     if ( formatsAndCodecs->isCodecAvailable( "Video", videoCodecList[ i ], &experimental ) == true )
     {
       qDebug() << "[vokoscreen] find Videocodec" << videoCodecList[ i ];
       myUi.VideocodecComboBox->addItem( videoCodecList[ i ], experimental );
     }
     else
     {
       qDebug() << "[vokoscreen] not found Videocodec" << videoCodecList[ i ];
     }
   }
   // Fallback
   int x = myUi.VideocodecComboBox->findText( vkSettings.getVideoCodec() );
   if ( x == -1 )
      myUi.VideocodecComboBox->setCurrentIndex( 0 );
   else
      myUi.VideocodecComboBox->setCurrentIndex( x );
   qDebug() << "[vokoscreen] ---End search video codec---";
   qDebug( " " );

   
   qDebug() << "[vokoscreen] ---Begin search audio codec---";
   QStringList audioCodecList;
   audioCodecList << "libmp3lame" << "libvorbis" << "pcm_s16le" << "libvo_aacenc" << "aac";
   for ( int i = 0; i < audioCodecList.count(); i++ )
   {
     if ( formatsAndCodecs->isCodecAvailable( "Audio", audioCodecList[ i ], &experimental ) == true )
     {
       qDebug() << "[vokoscreen] find Audiocodec" << audioCodecList[ i ];
       myUi.AudiocodecComboBox->addItem( audioCodecList[ i ], experimental );
     }
     else
     {
       qDebug() << "[vokoscreen] not found Audiocodec" << audioCodecList[ i ];
     }
   }
   // Fallback
   x = myUi.AudiocodecComboBox->findText( vkSettings.getAudioCodec() );
   if ( x == -1 )
      myUi.AudiocodecComboBox->setCurrentIndex( 0 );
   else
      myUi.AudiocodecComboBox->setCurrentIndex( x );
   qDebug() << "[vokoscreen] ---End search audio codec---";
   qDebug( " " );

   
   qDebug() << "[vokoscreen] ---Begin search formats---";
   QStringList formatList   = ( QStringList() << "mkv"      << "mp4" << "gif" );
   QStringList userDataList = ( QStringList() << "matroska" << "mp4" << "gif" );
   for ( int i = 0; i < formatList.count(); i++ )
   {
     if ( formatsAndCodecs->isFormatAvailable( userDataList[ i ] ) == true )
     {
       qDebug() << "[vokoscreen] find Format" << formatList[ i ];
       myUi.VideoContainerComboBox->addItem( formatList[ i ], userDataList[ i ] );
     }
     else
       qDebug() << "[vokoscreen] not found Format" << formatList[ i ];
   }
   // Fallback
   x = myUi.VideoContainerComboBox->findText( vkSettings.getVideoContainer() );
   if ( x == -1 )
      myUi.VideoContainerComboBox->setCurrentIndex( 0 );
   else
      myUi.VideoContainerComboBox->setCurrentIndex( x );
   qDebug() << "[vokoscreen] ---End search formats---";
   qDebug( " " );

   
   qDebug() << "[vokoscreen] ---Begin search devices---";
     QString device = "x11grab";
     if ( formatsAndCodecs->isDeviceAvailable( device ) == true ) 
       qDebug() << "[vokoscreen] find device" << device;
     else
       qDebug() << "[vokoscreen] not found device" << device;
   qDebug() << "[vokoscreen] ---End search devices---";
   qDebug( " " );
   
   clickedScreenSize();
   AreaOnOff();
}

screencast::~screencast()
{ 
}


void screencast::WindowMinimized()
{
  setWindowState( Qt::WindowMinimized );
}


/**
 * Wird beim beenden von vokoscreen aufgerufen
 */
void screencast::closeEvent( QCloseEvent * event )
{
  (void)event;
  if ( myUi.pointerQCheckBox->checkState() == Qt::Checked )
    myUi.pointerQCheckBox->click();
  Stop();
  //saveSettings();
  myregionselection->close();
  magnifier->close();
  webcamController->webcamCloseEvent();  
  SystemTrayIcon->hide();  
}


#ifndef NO_NEW_VERSION_CHECK
void screencast::buttonVersion()
{
  QString localVersion = vkSettings.getVersion();
  if ( version.isNewVersionAvailable( localVersion, version.getRemoteVersion() ) )
    myUi.updateButton->show();
  else
    myUi.updateButton->hide();
}
#endif


void screencast::addLogVokoscreen( QString value )
{
  myUi.ListWidgetLogVokoscreen->addItem( value );
  myUi.ListWidgetLogVokoscreen->scrollToBottom();
}

void screencast::VisibleHideKonsole()
{
  if ( myUi.ListWidgetLogVokoscreen->isVisible() )
  {
    myUi.ListWidgetLogVokoscreen->setVisible( false );
    resize( width(), oldMainWindowHeight );
  }
  else
  {
    myUi.ListWidgetLogVokoscreen->setVisible( true );
    resize( width(), 600 );
  }
}

#ifdef QT5
#include <QScreen>
#endif 
void screencast::myScreenCountChanged( int newCount )
{
    (void)newCount;
    myUi.ScreenComboBox->clear();
    QDesktopWidget *desk = QApplication::desktop();
    qDebug() << "[vokoscreen]" << "---Begin search Screen---";
    qDebug() << "[vokoscreen]" << "Number of screens:" << desk->screenCount();
    qDebug() << "[vokoscreen] Primary screen is: Display" << desk->primaryScreen()+1;
    qDebug() << "[vokoscreen] VirtualDesktop:" << desk->isVirtualDesktop();
  #ifdef QT5
      //QList < QScreen *> screens = QGuiApplication::screens();
      QScreen *screen = QGuiApplication::primaryScreen();    
      qDebug() << "[vokoscreen] DevicePixelRatio:" << screen->devicePixelRatio() << " (On normal displays is 1 and on Retina is 2)";
  #endif
    
    for ( int i = 1; i < desk->screenCount()+1; i++ )
    {
      QString ScreenGeometryX1 = QString::number( desk->screenGeometry( i-1 ).left() );
      QString ScreenGeometryY1 = QString::number( desk->screenGeometry( i-1 ).top() );
      #ifdef QT4
        QString ScreenGeometryX = QString::number( desk->screenGeometry( i-1 ).width() );
        QString ScreenGeometryY = QString::number( desk->screenGeometry( i-1 ).height() );
      #endif
      #ifdef QT5
        QString ScreenGeometryX = QString::number( desk->screenGeometry( i-1 ).width() * screen->devicePixelRatio() ); // devicePixelRatio() for Retina Displays
        QString ScreenGeometryY = QString::number( desk->screenGeometry( i-1 ).height() * screen->devicePixelRatio() );
      #endif
      myUi.ScreenComboBox->addItem( tr( "Display" ) + " " + QString::number( i ) + ":  " + ScreenGeometryX + " x " + ScreenGeometryY, i-1 );
      qDebug() << "[vokoscreen]" << "Display " + QString::number( i ) + ":  " + ScreenGeometryX + " x " + ScreenGeometryY;
    }
    myUi.ScreenComboBox->addItem( tr( "All Displays" ), -1 );    
    qDebug() << "[vokoscreen]" << "---End search Screen---";
    qDebug( " " );
}

void screencast::showMagnifier()
{
  if ( myUi.MagnifierCheckBox->isChecked() )
    magnifier-> magnifierShow();
  else
    magnifier->close(); 
}


void screencast::uncheckMagnifier()
{
  if ( myUi.MagnifierCheckBox->checkState() == Qt::Checked )
    myUi.MagnifierCheckBox->click();
}

void screencast::ShortcutPause()
{
  myUi.PauseButton->click();
}


void screencast::AreaOnOff()
{
  if ( myUi.FullScreenRadioButton->isChecked() or myUi.WindowRadioButton->isChecked() )
    myregionselection->close();

  if ( myUi.AreaRadioButton->isChecked() )
  {
    myregionselection->close();
    myregionselection->show();
  }
}


QString screencast::getOsRelease()
{
  QString OS;
  QString ID;
  QString VersionID;
  QString content;
  QFile file("/etc/os-release");

  if ( file.exists() )
  {
    file.open(QIODevice::ReadOnly);
      content = file.readAll().constData();
      QStringList list = content.split( "\n" );
      QStringList listID = list.filter( QRegExp( "^ID=" ) );
      if ( !listID.empty() )
        ID = listID[0].remove(0, 3).replace("\"", ""  );
      
      QStringList listVersionID = list.filter( QRegExp( "^VERSION_ID=" ) );
      if ( !listVersionID.empty() )
	VersionID = listVersionID[0].remove( 0, 11 ).replace("\"", ""  );
    
    OS = ID + " " + VersionID;
      
    file.close();
  }
  else
  {
    OS = "/etc/os-release not found";
  }
 
  return OS;
}


/**
 * Looking for external programs
 */
void screencast::searchExternalPrograms()
{
  qDebug() << "[vokoscreen]" << "---Begin Search external tools---";
  
  if ( searchProgramm( vkSettings.getRecorder() ) )
    qDebug() << "[vokoscreen]" << "Search ffmpeg ..... found" << vkSettings.getRecorder() << "Version:" << getFfmpegVersion();
  else
    qDebug() << "[vokoscreen]" << "Search ffmpeg ..... not found";
  
  if ( searchProgramm("pactl") )
     qDebug() << "[vokoscreen]" << "Search pactl  ..... found Version:" << getPactlVersion();
  else
     qDebug() << "[vokoscreen]" << "Error: pactl is not found, this is an PulseAudio-utils tool. Please install pactl";

  qDebug() << "[vokoscreen]" << "---End search external tools---";
  qDebug( " " );
}


/**
 * Search program foo in PATH
 */
bool screencast::searchProgramm( QString ProgName )
{
    bool find = false;
    QString prog;
    
    // if ProgName with path
    if ( ProgName.contains("/", Qt::CaseInsensitive) and ( QFile::exists( ProgName ) ) )
      return true;
      
    QString resultString( qgetenv( "PATH" ) );
    QStringList pathList = resultString.split( ":" );
      for ( int i = 0; i < pathList.size(); ++i )
      {
        prog = pathList.at( i ) + QDir::separator() + ProgName;
        if ( QFile::exists( prog ) )
        {
          find = true;
          break;
        }
      }
    return find;
}


QString screencast::getFfmpegVersion()
{
  QProcess Process;
  Process.start( vkSettings.getRecorder() + " -version");
  Process.waitForFinished();
  QString ffmpegversion = Process.readAllStandardOutput();
  Process.close();
  
  QStringList list = ffmpegversion.split( "\n" );
  if ( list.empty() )
    ffmpegversion = "";
  else
    ffmpegversion = list[ 0 ];

  return ffmpegversion;
}


QString screencast::getPactlVersion()
{
  QProcess Process;
  Process.start("pactl --version");
  Process.waitForFinished();
  QString pactlVersion = Process.readAllStandardOutput();
  Process.close();

  QStringList list = pactlVersion.split( "\n" );
  list = list[ 0 ].split( " " );
  return list[ 1 ];
}


/*
 * Setzt neues Icon um aufzuzeigen das Audio abgeschaltet ist
 */
void screencast::AudioOff( int state )
{
  if ( state == Qt::Unchecked )  
  {
    QIcon myIcon = myUi.tabWidget->tabIcon( 1 );
    QSize size = myUi.tabWidget->iconSize();
    QPixmap workPixmap( myIcon.pixmap( size ) );
    QPainter painter;
    QPen pen;
    painter.begin( &workPixmap );
      pen.setColor( Qt::red );
      pen.setWidth( 2 );
      painter.setPen( pen );
      painter.drawLine ( 5, 5, size.width()-5, size.height()-5 );
      painter.drawLine ( 5, size.height()-5, size.width()-5, 5 );
    painter.end();
    myUi.tabWidget->setTabIcon( 1, QIcon( workPixmap ) );
  }
  else{
    myUi.tabWidget->setTabIcon( 1, QIcon::fromTheme( "audio-input-microphone", QIcon( ":/pictures/micro.png" ) ) );
  }
}


/**
 * Returns Checkbox from Pulse device
 * 
 */
QCheckBox * screencast::getCheckBoxPulseDevice( int value )
{
//  QList<QCheckBox *> listQFrame = Pulseframe->findChildren<QCheckBox *>();
  QList<QCheckBox *> listQFrame = PulseFrame->findChildren<QCheckBox *>();  
  QCheckBox *inBox;  
  inBox = listQFrame.at( value );
  return inBox;
}


/**
 * CardxList beinhaltet "card0", "card1" ...
 * */
void screencast::AlsaWatcherEvent( QStringList CardxList )
{
  qDebug() << "[vokoscreen] ---Begin search Alsa capture device---";

  myUi.AlsaHwComboBox->clear();
  AlsaDeviceList.clear();
  // Für jede card wird eine Instanz erzeugt und in AlsaDeviceList abgelegt
  for( int i = 0; i <= CardxList.count() - 1; i++ )
  {
    QvkAlsaDevice * alsaDevice = new QvkAlsaDevice( CardxList[ i ] );
    AlsaDeviceList.append( alsaDevice );
    myUi.AlsaHwComboBox->addItem( AlsaDeviceList.at( i )->getAlsaName() , i );
  }

  QSettings settings( vkSettings.getProgName(), vkSettings.getProgName() );
  settings.beginGroup( "Alsa" );
    int x = myUi.AlsaHwComboBox->findText( settings.value( "NameCaptureCard" ).toString(),Qt::MatchExactly );
    myUi.AlsaHwComboBox->setCurrentIndex( x );
  settings.endGroup();
  qDebug() << "[vokoscreen] ---End search Alsa capture device---";
  qDebug( " " );

  settings.beginGroup( "Pulse" );
    PulseMultipleChoice();
    /*for ( int x = 0; x < 10; x++ )
       for ( int i = 0; i < QvkPulse::getPulseInputDevicesCount(); i++ )
       {
          QCheckBox *aa = getCheckBoxPulseDevice( i );
          if ( aa->text() == settings.value( "NameCaptureCard-" + QString::number( x + 1 ) ).toString() )
            aa->setCheckState( Qt::Checked );
       }  */
  settings.endGroup();
}

/**
 * Erstellt eine Scrollarea mit einem Frame
 * in dem die Checkboxen gesetzt werden
 * 
 * In setAccessibleName steht das Pulse Device
 */
void screencast::PulseMultipleChoice()
{
  qDebug() << "[vokoscreen]" << "---Begin search PulseAudio Capture Devices---";
 
  QList<QCheckBox *> listQScrollArea = myUi.scrollAreaWidgetContents->findChildren<QCheckBox *>();
  for ( int i = 0; i < listQScrollArea.count(); ++i )
  {
    delete listQScrollArea[ i ];
  }
  
  for ( int i = 1; i <= QvkPulse::getPulseInputDevicesCount(); ++i )
  {
    namePulse = new QCheckBox();
    myUi.verticalLayout_3->addWidget( namePulse );
    namePulse->setText( QvkPulse::getPulseInputName( i ) );
    namePulse->setAccessibleName( QvkPulse::getPulseInputDevices( i  ) );
    namePulse->setToolTip( tr ( "Select one or more devices" ) );
    qDebug() << "[vokoscreen]" << "Find CaptureCard:" << namePulse->text() << "with device:" << namePulse->accessibleName();
  }

  AudioOnOff();

  qDebug() << "[vokoscreen]" << "---End search PulseAudio Capture Devices---";
  qDebug( " " );
}


void screencast::AudioOnOff()
{
  if ( myUi.AudioOnOffCheckbox->checkState() == Qt::Checked )
  {
    myUi.AlsaRadioButton->setEnabled( true );
    myUi.PulseDeviceRadioButton->setEnabled( true );
    
    if ( myUi.PulseDeviceRadioButton->isChecked() )
      myUi.scrollArea->setEnabled( true );
    else
      myUi.scrollArea->setEnabled( false );
    
    if ( myUi.AlsaRadioButton->isChecked() )
      myUi.AlsaHwComboBox->setEnabled( true );
    else
      myUi.AlsaHwComboBox->setEnabled( false );
    
    myUi.AudiocodecComboBox->setEnabled( true );
  }
  else
  {
    myUi.AlsaRadioButton->setEnabled( false );
    myUi.AlsaHwComboBox->setEnabled( false );
    myUi.scrollArea->setEnabled( false );
    myUi.PulseDeviceRadioButton->setEnabled( false );
    myUi.AudiocodecComboBox->setEnabled( false );
  }
}

/**
 * Statusbar
 */
void screencast::valueChangedFrames( int i ) 
{
  (void)i;
  statusBarLabelFpsSettings->setText( QString::number( myUi.FrameSpinBox->value() ) );
}
  

/**
 * Statusbar
 */
void screencast::stateChangedAudio( int state )
{
  if ( state == Qt::Unchecked )
     statusBarLabelAudio->setText( "off" );

  if ( state == Qt::Checked )
  {
     if ( myUi.AlsaRadioButton->isChecked() )    
       statusBarLabelAudio->setText( "Alsa" );
     
     if ( myUi.PulseDeviceRadioButton->isChecked() )
       statusBarLabelAudio->setText( "Pulse" );
  }
}


/**
 * Statusbar
 */
void screencast::clickedAudioAlsa( bool checked ) 
{
  if ( checked )
     statusBarLabelAudio->setText( "Alsa" );
}


/**
 * Statusbar
 */
void screencast::clickedAudioPulse( bool checked )
{
  if ( checked == true ) 
    statusBarLabelAudio->setText( "Pulse" );
}


/**
 * Statusbar
 */
void screencast::currentIndexChangedCodec( int index )
{
  (void)index;
  statusBarLabelCodec->setText( myUi.VideocodecComboBox->currentText() );

  if ( myUi.VideocodecComboBox->currentText() == "gif" )
  {
    myUi.VideoContainerComboBox->setCurrentIndex( myUi.VideoContainerComboBox->findText( "gif" ) );
    if ( myUi.AudioOnOffCheckbox->checkState() == Qt::Checked )
    {
      myUi.AudioOnOffCheckbox->click();
    }
    
    myUi.AudioOnOffCheckbox->setEnabled( false );
  }

  if ( ( myUi.VideocodecComboBox->currentText() != "gif" ) and ( myUi.VideoContainerComboBox->currentText() != "gif" ) )
  {
    myUi.AudioOnOffCheckbox->setEnabled( true );
  } 
}


/**
 * Statusbar
 */
void screencast::currentIndexChangedFormat( int index )
{
  (void)index;
  statusBarLabelFormat->setText( myUi.VideoContainerComboBox->currentText() );
 
  if ( myUi.VideoContainerComboBox->currentText() == "gif" )
  {
    myUi.GIFplayerComboBox->show();
    myUi.VideoplayerComboBox->hide();
    myUi.VideocodecComboBox->setCurrentIndex( myUi.VideocodecComboBox->findText( "gif" ) );
    myUi.VideocodecComboBox->setEnabled( false );
    if ( myUi.AudioOnOffCheckbox->checkState() == Qt::Checked )
    {
      myUi.AudioOnOffCheckbox->click();
    }
    
    myUi.AudioOnOffCheckbox->setEnabled( false );
  }
  else
  {
    myUi.GIFplayerComboBox->hide();
    myUi.VideoplayerComboBox->show();
    myUi.VideocodecComboBox->setEnabled( true );
    if ( myUi.VideocodecComboBox->currentText() == "gif" )
      myUi.VideocodecComboBox->setCurrentIndex( 0 ); 
  }
 
  if ( ( myUi.VideocodecComboBox->currentText() != "gif" ) and ( myUi.VideoContainerComboBox->currentText() != "gif" ) )
  {
    myUi.AudioOnOffCheckbox->setEnabled( true );
  }
}


void screencast::setRecordWidth( QString value )
{
  screenRecordWidth = value; 
}


QString screencast::getRecordWidth()
{
  return screenRecordWidth; 
}

void screencast::setRecordHeight( QString value )
{
  screenRecordHeight = value; 
}


QString screencast::getRecordHeight()
{
  return screenRecordHeight; 
}

/**
 * Statusbar
 */
void screencast::clickedScreenSize()
{
  if ( myUi.FullScreenRadioButton->isChecked() )
  {
    if ( SystemCall->state() == QProcess::Running )
      statusbarLabelScreenSize->setText( "F:" + getRecordWidth() + "x" + getRecordHeight() );
    else
      statusbarLabelScreenSize->setText( "F" );
    
    if ( ( SystemCall->state() == QProcess::Running ) or ( myUi.PauseButton->isChecked() ) )
      myUi.ScreenComboBox->setEnabled( false );
    else
      myUi.ScreenComboBox->setEnabled( true );
  }
  
  if ( myUi.WindowRadioButton->isChecked() )
  {
    if ( SystemCall->state() == QProcess::Running )
      statusbarLabelScreenSize->setText( "W:" + getRecordWidth() + "x" + getRecordHeight() );
    else
      statusbarLabelScreenSize->setText( "W" );
    
    myUi.ScreenComboBox->setEnabled( false );
  }
  
  if ( myUi.AreaRadioButton->isChecked() )
  {
    if ( SystemCall->state() == QProcess::Running )
      statusbarLabelScreenSize->setText( "A:" + getRecordWidth() + "x" + getRecordHeight() );
    else
      statusbarLabelScreenSize->setText( "A" );
    
    myUi.ScreenComboBox->setEnabled( false );
  }
}


/**
 * Set standard video codec and options
 */
void screencast::setVideocodecStandardComboBox()
{
  myUi.VideocodecComboBox->setCurrentIndex( myUi.VideocodecComboBox->findText( "libx264", Qt::MatchExactly ) );
  myUi.VideoContainerComboBox->setCurrentIndex( myUi.VideoContainerComboBox->findText( "mkv", Qt::MatchExactly ) );
}


/**
 * Set standard Audio codec
 */
void screencast::setAudiocodecStandardComboBox()
{
  myUi.AudiocodecComboBox ->setCurrentIndex( myUi.AudiocodecComboBox->findText( "libmp3lame", Qt::MatchExactly )  );
}


/**
 * Set standard fps
 */
void screencast::setFrameStandardSpinbox()
{
  myUi.FrameSpinBox->setValue( 25 );
}


void screencast::searchGIFPlayer()
{
    qDebug() << "[vokoscreen]" << "---Begin search GIFplayer---";
    QStringList GIFList = QStringList()  << "firefox"
                                         << "mpv"
					 << "chromium"
					 << "konqueror";
    for ( int x = 0; x < GIFList.size(); ++x )
    {
      if ( searchProgramm( GIFList[ x ] ) == true )
      {
        qDebug() << "[vokoscreen]" << "Find GIFplayer :" << GIFList[ x ];
	myUi.GIFplayerComboBox->addItem( GIFList.at( x ), GIFList.at( x ) );
      }	
    }
    qDebug() << "[vokoscreen]" << "---End search GIFoplayer---";
    qDebug( " " );
}


void screencast::searchVideoPlayer()
{
    qDebug() << "[vokoscreen]" << "---Begin search Videoplayer---";
    QStringList playerList = QStringList()  << "vlc"
                                            << "kaffeine"
                                            << "gnome-mplayer"
                                            << "totem"
                                            << "pia"
                                            << "xine"
                                            << "gxine"
                                            << "gmplayer"
                                            << "kplayer"
                                            << "smplayer"
                                            << "smplayer2"
                                            << "dragon"
                                            << "banshee"
					    << "openshot"
					    << "kdenlive"
					    << "mpv";

    QString playerName;
    QString resultString( qgetenv( "PATH" ) );
    QStringList pathList = resultString.split( ":" );
    for ( int x = 0; x < playerList.size(); ++x )
     {
       for ( int i = 0; i < pathList.size(); ++i )
       {
         playerName = pathList.at( i );
         playerName = playerName.append( QDir::separator() ).append( playerList.at( x ) );
         if ( QFile::exists( playerName ) )
         {
           qDebug() << "[vokoscreen]" << "Find Videoplayer :" << playerName;
	   QFileInfo playProg( playerName );
	   if ( playProg.fileName() == "kdenlive" )
	     playerName = playerName + " -i";

	   myUi.VideoplayerComboBox->addItem( playerList.at( x ), playerName );
           break;
         }
       }
     }
    qDebug() << "[vokoscreen]" << "---End search Videoplayer---";
    qDebug( " " );
}


#ifdef QT4
void screencast::saveVideoPath()
{
  QString dir = QFileDialog::getExistingDirectory( this, tr( "Open Directory" ),
                QDesktopServices::storageLocation( QDesktopServices::HomeLocation ), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks );

  if ( dir > "" )
    myUi.SaveVideoPathLineEdit->setText( dir );
}
#endif

#ifdef QT5
void screencast::saveVideoPath()
{
  QString dir = QFileDialog::getExistingDirectory( this, tr( "Open Directory" ),
                QStandardPaths::writableLocation( QStandardPaths::HomeLocation ), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks );

  if ( dir > "" )
      myUi.SaveVideoPathLineEdit->setText( dir );
}
#endif


void screencast::readyReadStandardError()
{
  int s = beginTime.secsTo( QDateTime::currentDateTime() );
  int HH = s / 3600;
  int MM = ( s- ( s / 3600 ) * 3600 ) / 60;
  int SS = s % 60;
  QTime myTime( HH, MM, SS);
  QString time = myTime.toString ( "hh:mm:ss");
  statusBarLabelTime->setText( time );

  QString output = SystemCall->readAllStandardError();
  if ( output.contains( "fps=", Qt::CaseInsensitive ) )
  {
    int x = output.indexOf( "fps" );
    statusBarLabelFps->setText( output.mid( x + 4, 3 ).replace( " ", "" ) );
  }

  QFileInfo fileInfo( PathTempLocation() + QDir::separator() + nameInMoviesLocation );
  statusBarLabelSize->setText( QString::number( fileInfo.size() / 1024 ) );
}


/**
 * Je nach Prozessstaus von ffmpeg werden die Widget enabled bzw. disabled
 */
void screencast::stateChanged ( QProcess::ProcessState newState )
{
    if ( ( newState == QProcess::Running ) and ( pause == false) )
    {
      myUi.StopButton->setEnabled(true);
      myUi.PauseButton->setEnabled(true);
      myUi.recordButton->setEnabled(false);
      myUi.PlayButton->setEnabled(false);
      myUi.FullScreenRadioButton->setEnabled( false );
      myUi.WindowRadioButton->setEnabled( false );
      myUi.AreaRadioButton->setEnabled( false );
      clickedScreenSize();

      myUi.TabWidgetAudioFrame->setEnabled(false);
      //myUi.TabWidgetMiscellaneousFrame->setEnabled(false);
      myUi.tab_2->setEnabled( false );
      myUi.TabWidgetVideoOptionFrame->setEnabled( false );

      if ( myUi.SystrayCheckBox->checkState() == Qt::Checked )
      {
        SystemTrayIcon->setIcon( QIcon::fromTheme( "media-record", QIcon( ":/pictures/systray-record" ) ) );
	startAction->setEnabled( false );
	stopAction->setEnabled( true );
	pauseAction->setEnabled( true );
	goAction->setEnabled( false );
      } 
    }
  
    if ((newState == QProcess::NotRunning) and (pause == false))
    {
      myUi.StopButton->setEnabled(false);
      myUi.PauseButton->setEnabled(false);
      myUi.recordButton->setEnabled(true);
      myUi.FullScreenRadioButton->setEnabled( true );
      myUi.WindowRadioButton->setEnabled( true );
      myUi.AreaRadioButton->setEnabled( true );
      clickedScreenSize();

      myUi.TabWidgetAudioFrame->setEnabled(true);
//      myUi.TabWidgetMiscellaneousFrame->setEnabled(true);
      myUi.tab_2->setEnabled(true);

      myUi.TabWidgetVideoOptionFrame->setEnabled( true );
      
      if ( myUi.SystrayCheckBox->checkState() == Qt::Checked )
      {
        SystemTrayIcon->setIcon( QIcon(":/pictures/systray.png" ) );
	startAction->setEnabled( true );
	stopAction->setEnabled( false );
	pauseAction->setEnabled( false );
	goAction->setEnabled( false );
      }
      
      if ( myUi.MagnifierCheckBox->isChecked() )
	myUi.MagnifierCheckBox->click();
    }
    
    if ((newState == QProcess::NotRunning) and (pause == true) and ( myUi.PauseButton->isChecked() ))
    {
      myUi.StopButton->setEnabled(false);
      myUi.PauseButton->setEnabled(true);
      myUi.recordButton->setEnabled(false);
      myUi.PlayButton->setEnabled(false);
      myUi.FullScreenRadioButton->setEnabled( false );
      myUi.WindowRadioButton->setEnabled( false );
      myUi.AreaRadioButton->setEnabled( false );
      clickedScreenSize();
      
      myUi.TabWidgetAudioFrame->setEnabled(false);
//      myUi.TabWidgetMiscellaneousFrame->setEnabled(false);
      myUi.tab_2->setEnabled(false);
      myUi.TabWidgetVideoOptionFrame->setEnabled( false );

      if ( myUi.SystrayCheckBox->checkState() == Qt::Checked )
      {
	SystemTrayIcon->setIcon( QIcon::fromTheme( "media-playback-pause", QIcon( ":/pictures/pause.png" ) ) );
	startAction->setEnabled( false );
	stopAction->setEnabled( false );
	pauseAction->setEnabled( false );
	goAction->setEnabled( true );
      } 
    }

    if ((newState == QProcess::Running) and (pause == true) and ( not myUi.PauseButton->isChecked() ))
    {
      myUi.StopButton->setEnabled(true);
      myUi.PauseButton->setEnabled(true);
      myUi.recordButton->setEnabled(false);
      myUi.PlayButton->setEnabled(false);
      myUi.FullScreenRadioButton->setEnabled( false );
      myUi.WindowRadioButton->setEnabled( false );
      myUi.AreaRadioButton->setEnabled( false );
      clickedScreenSize();
      
      myUi.TabWidgetAudioFrame->setEnabled(false);
//      myUi.TabWidgetMiscellaneousFrame->setEnabled(false);
      myUi.tab_2->setEnabled(false);
      myUi.TabWidgetVideoOptionFrame->setEnabled( false );

      if ( myUi.SystrayCheckBox->checkState() == Qt::Checked )
      {
        SystemTrayIcon->setIcon( QIcon::fromTheme( "media-record", QIcon( ":/pictures/systray-record" ) ) );
	startAction->setEnabled( false );
	stopAction->setEnabled( true );
	pauseAction->setEnabled( true );
	goAction->setEnabled( false );
      } 
    } 
      
    if ( ( newState == QProcess::NotRunning ) and ( pause == true ) and ( not myUi.PauseButton->isChecked() ) )
    {
      myUi.StopButton->setEnabled(false);
      myUi.PauseButton->setEnabled(false);
      myUi.recordButton->setEnabled(true);
      myUi.FullScreenRadioButton->setEnabled( true );
      myUi.WindowRadioButton->setEnabled( true );
      myUi.AreaRadioButton->setEnabled( true );
      clickedScreenSize()      ;
      
      myUi.TabWidgetAudioFrame->setEnabled(true);
//      myUi.TabWidgetMiscellaneousFrame->setEnabled(true);
      myUi.tab_2->setEnabled(true);
      myUi.TabWidgetVideoOptionFrame->setEnabled( true );

      if ( myUi.SystrayCheckBox->checkState() == Qt::Checked )
      {
        SystemTrayIcon->setIcon( QIcon(":/pictures/systray.png" ) );
	startAction->setEnabled( true );
	stopAction->setEnabled( false );
	pauseAction->setEnabled( false );
	goAction->setEnabled( false );
      } 
    }
       
    if ( newState == QProcess::Running )
    {
      qDebug() << "[vokoscreen]" << recordApplikation << "is running and is ready for reading and writing";
      qDebug( " " );
    }
   
    if ( newState == QProcess::NotRunning )
    {
      qDebug() << "[vokoscreen]" << recordApplikation << "is not running";
      qDebug( " " );

      //Enables the customarea rectangle again. (Is diabled in record() )
      if ( !myUi.PauseButton->isChecked() )
      {
        myregionselection->lockFrame( false );
      }
    }

    if ( ( myUi.VideocodecComboBox->currentText() == "gif" ) or ( myUi.VideoContainerComboBox->currentText() == "gif" ) )
    {
      myUi.PauseButton->setEnabled( false );
    } 

}


void screencast::error( QProcess::ProcessError error )
{
  // Kann mann testen in dem man den ffmpeg aus der Processliste tötet
  if ( error == QProcess::Crashed )
  {
    // remove crashed directory
    QSettings settings1( vkSettings.getProgName(), vkSettings.getProgName() );
    QFileInfo settingsPath1( settings1.fileName() );
    QFile file1( settingsPath1.absolutePath() );
    QString workDirectory1 = file1.fileName();
    QDir Dir1( "" );
    Dir1.setCurrent( workDirectory1 );
    if ( Dir1.cd( "crashed" ) )
    {
      QStringList stringList1 = Dir1.entryList( QDir::Files, QDir::NoSort );
      for ( int i = 0; i < stringList1.count(); ++i )
        Dir1.remove( stringList1[ i ] );
    }
    
    // Move ffmpeg.log, vokoscreen.log, vokoscreen.conf in directory crashed
    QSettings settings( vkSettings.getProgName(), vkSettings.getProgName() );
    QFileInfo settingsPath( settings.fileName() );
    QFile file( settingsPath.absolutePath() );
    QString workDirectory = file.fileName();
    QDir Dir( "" );
    Dir.setCurrent( workDirectory );
    Dir.mkdir( "crashed" );
    
    QStringList stringList = Dir.entryList( QDir::Files, QDir::NoSort );
    for ( int i = 0; i < stringList.count(); ++i )
      QFile::copy( workDirectory + QDir::separator() + stringList[ i ], workDirectory + QDir::separator() + "crashed" + QDir::separator() + stringList[ i ]);
    
    QString crashedtext = "[vokoscreen] " + recordApplikation + " is crashed on ";
    QDateTime dateTime = QDateTime::currentDateTime();
    QString time = dateTime.toString( "hh:mm" );
    QString day = dateTime.toString( "dddd" );
    qDebug( " " );
    qDebug() << "********************************************************";
    qDebug() << crashedtext + day + ", " + time;
    qDebug() << "********************************************************";
    QMessageBox msgBox;
    msgBox.setIcon( QMessageBox::Critical );
    msgBox.setText( crashedtext + day + ", " + time );
    msgBox.exec();
  }

  // Noch nicht getestet
  if ( error == QProcess::FailedToStart )
  {
    qDebug() << "The process could not be started. Either the is called program is not installed, or the ffmpeg call Faulty or you have not over sufficient permissions to to the program.";
    QMessageBox msgBox;
    msgBox.setText( "The process could not be started. Either the is called program is not installed, or the ffmpeg call Faulty or you have not over sufficient permissions to to the program." );
    msgBox.exec();
  }
}

void screencast::Pause()
{
  if ( myUi.FullScreenRadioButton->isChecked() or myUi.AreaRadioButton->isChecked() )
  {
    pause = true;
    if ( myUi.PauseButton->isChecked() )
    {
      shortcutStop->setEnabled( false );
      windowMoveTimer->stop();
      myUi.PauseButton->setText( tr ( "Go" ) );
      SystemCall->terminate();
      SystemCall->waitForFinished();
      QvkPulse::pulseUnloadModule();
    }
    else
    {
      QVariant aa = myUi.AlsaHwComboBox->itemData( myUi.AlsaHwComboBox->currentIndex() );
      QvkAlsaDevice *inBox = AlsaDeviceList.at( aa.toInt() );
      if ( inBox->isbusy() and myUi.AlsaRadioButton->isChecked() )
      {
        inBox->busyDialog( inBox->getAlsaHw(), inBox->getPurAlsaName() );
	myUi.PauseButton->click();
        return;
      }
      Countdown();
      shortcutStop->setEnabled( true );
      myUi.PauseButton->setText( tr( "Pause" ) );
      startRecord( PathTempLocation() + QDir::separator() + PauseNameInTmpLocation() );
    }
  }
  
  
  if ( myUi.WindowRadioButton->isChecked() )
  {
    pause = true;
    if ( myUi.PauseButton->isChecked() )
    {
      shortcutStop->setEnabled( false );
      windowMoveTimer->stop();
      myUi.PauseButton->setText( tr ( "Go" ) );
      SystemCall->terminate();
      SystemCall->waitForFinished();
      QvkPulse::pulseUnloadModule();
    }
    else
    {
      QVariant aa = myUi.AlsaHwComboBox->itemData( myUi.AlsaHwComboBox->currentIndex() );
      QvkAlsaDevice *inBox = AlsaDeviceList.at( aa.toInt() );
      if ( inBox->isbusy() and myUi.AlsaRadioButton->isChecked() )
      {
        inBox->busyDialog( inBox->getAlsaHw(), inBox->getPurAlsaName() );
        myUi.PauseButton->click();
        return;
      }
      Countdown();
      shortcutStop->setEnabled( true );
      myUi.PauseButton->setText( tr( "Pause" ) );
      startRecord( PathTempLocation() + QDir::separator() + PauseNameInTmpLocation() );
      windowMoveTimer->start();
    }
  }
}


/**
 * Play last screencast with Videoplayer
 */
void screencast::play()
{
  if ( myUi.MagnifierCheckBox->isChecked() )
	myUi.MagnifierCheckBox->click();
  
  if ( myUi.VideoplayerComboBox->count() == 0 )
  {
    QDialog *newDialog = new QDialog;
    newDialog->setModal( true );
    Ui_NoPlayerDialog myUiDialog;
    myUiDialog.setupUi( newDialog );
    newDialog->show();
    return;
  }
  
  QDir Dira( PathMoviesLocation() );
  QStringList filters;
  filters << "vokoscreen*";
  QStringList List = Dira.entryList( filters, QDir::Files, QDir::Time );
  QString player;
  QFileInfo fileinfo( List.at( 0 ) );
  if ( fileinfo.suffix() == "gif" )
  {
    QVariant aa = myUi.GIFplayerComboBox->itemData( myUi.GIFplayerComboBox->currentIndex() ); // get userdata from ComboBox
    player = aa.toString();
    player = player.replace( "\n", "" ); 
  }
  else
  {
    QVariant aa = myUi.VideoplayerComboBox->itemData( myUi.VideoplayerComboBox->currentIndex() ); // get userdata from ComboBox
    player = aa.toString();
    player = player.replace( "\n", "" ); 
  }
    
  QProcess *SystemCall = new QProcess();
  SystemCall->start( player + " " + '"' + PathMoviesLocation() + QDir::separator() + List.at( 0 ) + '"' );
}



QString screencast::PathMoviesLocation()
{
  QString Path;
  if ( myUi.SaveVideoPathLineEdit->displayText() > "" )
     Path = myUi.SaveVideoPathLineEdit->displayText();
  else
  { 
    #ifdef QT4
    if ( QDesktopServices::storageLocation( QDesktopServices::MoviesLocation).isEmpty() )
    #endif
    #ifdef QT5
    if ( QStandardPaths::writableLocation( QStandardPaths::MoviesLocation ).isEmpty() )
    #endif 
    {
       #ifdef QT4
       Path = QDesktopServices::storageLocation( QDesktopServices::HomeLocation );
       #endif
       #ifdef QT5
       Path = QStandardPaths::writableLocation( QStandardPaths::HomeLocation );
       #endif
       myUi.SaveVideoPathLineEdit->setText(Path);
    }
    else
    {
      #ifdef QT4
      Path = QDesktopServices::storageLocation( QDesktopServices::MoviesLocation );
      #endif
      #ifdef QT5
      Path = QStandardPaths::writableLocation( QStandardPaths::MoviesLocation );
      #endif
      myUi.SaveVideoPathLineEdit->setText( Path );
    }
  }
  return Path;
}


void screencast::myVideoFileSystemWatcher( const QString & path )
{
  (void)path;
  QDir Dira( PathMoviesLocation() );
  QStringList filters;
  filters << "vokoscreen*";
  QStringList List = Dira.entryList( filters, QDir::Files, QDir::Time );
  
  if ( List.isEmpty() )
  {
    myUi.PlayButton->setEnabled( false );
    myUi.sendPushButton->setEnabled( false );
  }
  else
  {
    myUi.PlayButton->setEnabled( true );
    myUi.sendPushButton->setEnabled( true );
  }
}


QString screencast::getFileWithPath( QString ProgName )
{
   if ( ProgName.contains("/", Qt::CaseInsensitive) and ( QFile::exists( ProgName ) ) )
    return ProgName; 
     
    QString find;
    QString prog;
    QString resultString( qgetenv( "PATH" ) );
    QStringList pathList = resultString.split( ":" );
    for ( int i = 0; i < pathList.size(); ++i )
    {
      prog = pathList.at( i ) + QDir::separator() + ProgName;
      if ( QFile::exists( prog ) )
      {
        find = prog;
        break;
      }
    }
    return find;
}

void screencast::recorderLineEditTextChanged( QString recorder )
{
   recordApplikation = recorder;
}


#ifdef QT4
void screencast::selectRecorder()
{
  QString recorder = QFileDialog::getOpenFileName( this,
					           tr( "Select recorder" ),
					           QDesktopServices::storageLocation( QDesktopServices::HomeLocation ) );

  if ( recorder > "" )
    myUi.RecorderLineEdit->setText( recorder );
}
#endif

#ifdef QT5
void screencast::selectRecorder()
{
    QString recorder = QFileDialog::getOpenFileName( this,
                                 tr( "Select recorder" ),
                                 QStandardPaths::writableLocation( QStandardPaths::HomeLocation ) );

    if ( recorder > "" )
      myUi.RecorderLineEdit->setText( recorder );
}
#endif


void screencast::showCredits()
{
   myUi.creditsQPushButton->setEnabled( false );
   credits = new QvkCredits();
   credits->show();
   connect( credits, SIGNAL( closeCredits() ), SLOT( creditsCloseEvent() ) );
}


/**
 * Wird aufgerufen wenn das credits Fenster geschloßen und dabei
 * das SIGNAL closeCredits in der Klasse QvkCredits ausgelösst wird
 */
void screencast::creditsCloseEvent()
{
   myUi.creditsQPushButton->setEnabled( true );
   delete credits;
}


void screencast::send()
{
  QvkMail *vkMail = new QvkMail( this );
  (void)vkMail;
}



QString screencast::PathTempLocation()
{
  QString tmpName = vkSettings.getProgName() + "-" + qgetenv( "USER" );  

  #ifdef QT4
  QString tempPathProg = QDesktopServices::storageLocation ( QDesktopServices::TempLocation ) + QDir::separator() + tmpName;
  #endif

  #ifdef QT5
  QString tempPathProg = QStandardPaths::writableLocation( QStandardPaths::TempLocation ) + QDir::separator() + tmpName;
  #endif
  
  QDir dirTempPathProg( tempPathProg );
  if ( not dirTempPathProg.exists() )
  {
      #ifdef QT4
      QString tempPath = QDesktopServices::storageLocation ( QDesktopServices::TempLocation );
      #endif
      #ifdef QT5
      QString tempPath = QStandardPaths::writableLocation( QStandardPaths::TempLocation );
      #endif      
      QDir dirTempPath( tempPath );
      dirTempPath.mkdir( tmpName );
  }
  return tempPathProg;  
}


/**
 * Return the new screencastname
 */
QString screencast::NameInMoviesLocation()
{
  return "vokoscreen-" + QDateTime::currentDateTime().toString( "yyyy-MM-dd_hh-mm-ss" ) + "." + myUi.VideoContainerComboBox->currentText();
}


/**
 * Return the new pausename
 */
QString screencast::PauseNameInTmpLocation()
{
  QString myFilename = "screencast-pause";
  QString myFilenameExtension = "." + myUi.VideoContainerComboBox->currentText();
  QString myName = PathTempLocation() + QDir::separator() + myFilename + myFilenameExtension;

  QFile *myFile = new QFile( myName );
  uint x = 0;
  do
  {
    x++;
    myFile->setFileName( PathTempLocation() + QDir::separator() + myFilename + "-" + QString().number( x ) + myFilenameExtension );
    myName = myFile->fileName();
  } while ( myFile->exists() );

  return myFilename + "-" + QString().number( x ) + myFilenameExtension;
}


QString screencast::myAlsa()
{
  QString value;
  if ( myUi.AudioOnOffCheckbox->checkState() == Qt::Checked )
  {
    if ( myUi.AlsaRadioButton->isChecked() )  
    {
      QVariant aa = myUi.AlsaHwComboBox->itemData( myUi.AlsaHwComboBox->currentIndex() );
      QvkAlsaDevice *inBox = AlsaDeviceList.at( aa.toInt() );
      if ( myUi.AlsaHwComboBox->currentIndex() > -1 )
        value = "-f alsa -ac " + inBox->getChannel() + " -i " + inBox->getAlsaHw() + " ";
      else
        value = "";
    }
    
    if ( myUi.PulseDeviceRadioButton->isChecked() )      
    {
      QCheckBox *box;
      int counter = 0;
      //QList<QCheckBox *> listQFrame = myUi.Pulseframe->findChildren<QCheckBox *>(); // original
      QList<QCheckBox *> listQFrame = myUi.scrollAreaWidgetContents->findChildren<QCheckBox *>();
      
      if ( listQFrame.count() > 0 )
        for ( int i = 0; i < listQFrame.count(); i++ )
        {
          box = listQFrame.at( i );
          if ( box->checkState() == Qt::Checked  )
            counter++;
        }

        if (counter > 1)
        { 
          //value = "-f alsa -f pulse -i vokoscreenMix.monitor "; // alsa plugin
          value = "-f pulse -i vokoscreenMix.monitor";
	  
        }
        else if ( counter > 0 )
        {
          //value = "-f alsa -i pulse "; // with alsa plugin
          for ( int i = 0; i < listQFrame.count(); i++ )
          {
            box = listQFrame.at( i );
            if ( box->checkState() == Qt::Checked  )
              value = "-f pulse -name vokoscreen -i " + box->accessibleName();
          }
        }
        
    }
  }
  else
      value = "";

  return value;
}


QString screencast::myAcodec()
{
  if ( ( myUi.AudioOnOffCheckbox->checkState() == Qt::Checked ) and ( myUi.AlsaRadioButton->isChecked() ) and ( myUi.AlsaHwComboBox->currentText() > "" ) )
  {
    if ( myUi.AudiocodecComboBox->itemData( myUi.AudiocodecComboBox->currentIndex() ) == true )
     return "-c:a " + myUi.AudiocodecComboBox->currentText() + " -strict experimental";
    else
     return "-c:a " + myUi.AudiocodecComboBox->currentText();
  }
  //if ( ( myUi.AudioOnOffCheckbox->checkState() == Qt::Checked ) and ( myUi.PulseDeviceRadioButton->isChecked() ) and ( QvkPulse::myPulseDevice( myUi.Pulseframe ) > "" ) )// original
  if ( ( myUi.AudioOnOffCheckbox->checkState() == Qt::Checked ) and ( myUi.PulseDeviceRadioButton->isChecked() ) and ( QvkPulse::myPulseDevice( myUi.verticalLayout_3 ) > "" ) )
  {
    if ( myUi.AudiocodecComboBox->itemData( myUi.AudiocodecComboBox->currentIndex() ) == true )
     return "-c:a " + myUi.AudiocodecComboBox->currentText() + " -strict experimental";
    else
     return "-c:a " + myUi.AudiocodecComboBox->currentText();
  }

  return "";
}


QString screencast::noMouse()
{
  if ( myUi.HideMouseCheckbox->checkState() == Qt::Checked  )
    return "-draw_mouse 0";
  else
    return "-draw_mouse 1";
}


void screencast::preRecord()
{
  if ( myUi.AlsaRadioButton->isChecked() and myUi.AudioOnOffCheckbox->isChecked() )
  {
    QVariant aa = myUi.AlsaHwComboBox->itemData( myUi.AlsaHwComboBox->currentIndex() );
    QvkAlsaDevice *inBox = AlsaDeviceList.at( aa.toInt() );
    if ( inBox->isbusy() )
    {
      inBox->busyDialog( inBox->getAlsaHw(), inBox->getPurAlsaName() );
      return;
    }
    else
    {
      // Kanäle werden kurz vor der Aufnahme ermittelt
      inBox->setChannel();
    }
  }
  
  if ( myUi.WindowRadioButton->isChecked() )
    if ( firststartWininfo == false )
    {
      vkWinInfo = new QvkWinInfo();
      connect( vkWinInfo, SIGNAL( windowChanged() ), this, SLOT( record() ) );
      return;
    }
   record();
}


void screencast::Countdown()
{
  if ( myUi.CountdownSpinBox->value() > 0 )
  {
    myUi.recordButton->setEnabled( false );
    myUi.PauseButton->setEnabled( false );
    QvkCountdown *countdown = new QvkCountdown( myUi.CountdownSpinBox->value() );
    (void)countdown;
  } 
}


void screencast::record()
{
  Countdown();
  shortcutStart->setEnabled( false );
  shortcutStop->setEnabled( true );
  if ( myUi.MinimizedCheckBox->checkState() == Qt::Checked )
    WindowMinimized();

  QString deltaX = "0";
  QString deltaY = "0";
  
  if ( myUi.WindowRadioButton->isChecked() and ( firststartWininfo == false) )
  {
      qDebug() << "[vokoscreen]" << "recording window";
      setRecordWidth( vkWinInfo->width() );
      setRecordHeight( vkWinInfo->height() );
      deltaX = QString::number( vkWinInfo->x().toUInt() );
      deltaY = QString::number( vkWinInfo->y().toUInt() );

      moveWindowID = vkWinInfo->getWinID();
      
      deltaXMove = deltaX;
      deltaYMove = deltaY;

      windowMoveTimer->start( 120 );
      firststartWininfo = true;
  }

  if( myUi.FullScreenRadioButton->isChecked() )
  {
      int screen = myUi.ScreenComboBox->itemData( myUi.ScreenComboBox->currentIndex() ).toInt();
      qDebug() << "[vokoscreen]" << "recording fullscreen Display: " << screen;

      int fullScreenWidth = 0;
      int fullScreenHeight = 0;

      QDesktopWidget *desk = QApplication::desktop();
      for ( int i = 0; i < desk->screenCount(); i++ )
      {
          // skip if we are recording a specific screen and this isn't it.
          if( screen != -1 && screen != i ) continue;
          // set the offset if we are recording a specific screen.
          if( screen != -1 )
          {
              deltaX = QString::number( desk->screenGeometry( i ).left() );
              deltaY = QString::number( desk->screenGeometry( i ).top() );
          }
          fullScreenWidth += desk->screenGeometry( i ).width();
          fullScreenHeight = std::max( fullScreenHeight, desk->screenGeometry( i ).height() );
      }
   #ifdef QT4
      setRecordWidth( QString::number( fullScreenWidth ) );
      setRecordHeight( QString::number( fullScreenHeight) );
   #endif

   #ifdef QT5
      QScreen *qscreen = QGuiApplication::primaryScreen();    
      setRecordWidth( QString::number( fullScreenWidth * qscreen->devicePixelRatio() ) );
      setRecordHeight( QString::number( fullScreenHeight * qscreen->devicePixelRatio() ) );
   #endif   
  }  

  if ( myUi.AreaRadioButton->isChecked() )
  {
    setRecordWidth( QString().number( myregionselection->getWidth() ) );
    setRecordHeight( QString().number( myregionselection->getHeight() ) );
    deltaX  = QString().number( myregionselection->getXRecordArea() );
    deltaY  = QString().number( myregionselection->getYRecordArea() );

    //Makes the rectangle unmovable and unresizeable (Is enabled yet again when process finished)
    myregionselection->lockFrame( true );

    qDebug() << "[vokoscreen]" << "recording area";
  }
  
  // set working directory for writing and delete the ffmpegLog from Profil directory
  QSettings settings( vkSettings.getProgName(), vkSettings.getProgName() );
  QFileInfo settingsPath( settings.fileName() );
  QFile file( settingsPath.absolutePath() );
  QString workDirectory = file.fileName();
  QDir Dir( "" );
  Dir.setCurrent( workDirectory );  
  
  // bestehendes ffmpegLog löschen
  QDir Dira( "" );
  QStringList filters;
  filters << "ffmpeg*";
  QStringList List = Dira.entryList(filters, QDir::Files, QDir::Name );
  QFile FfmpegFile;
  for (int i = 0; i < List.size(); ++i)
     FfmpegFile.remove( List.at(i) );

  // evtl. müßte hier erst mal nachgeschaut werden ob Temp leer ist.
  // Clean vokoscreen temp 
  QDir dir( PathTempLocation() );
  QStringList stringList = dir.entryList( QDir::Files, QDir::Time | QDir::Reversed );
  for ( int i = 0; i < stringList.size(); ++i )
     dir.remove( PathTempLocation().append(QDir::separator() ).append(stringList.at( i ) ) );

  // framerate
  QString framerate;
  framerate = "-framerate " + QString().number( myUi.FrameSpinBox->value() );

  QString myVcodec = myUi.VideocodecComboBox->currentText();
  if ( myVcodec == "libx264" )
  {
    // Number of pixels must be divisible by two
    int intRecordX = getRecordWidth().toInt();
    if ( ( intRecordX % 2 ) == 1 )
      setRecordWidth( QString().number( --intRecordX ) );

    // Number of pixels must be divisible by two
    int intRecordY = getRecordHeight().toInt();
    if ( ( intRecordY % 2 ) == 1 )
      setRecordHeight( QString().number( --intRecordY ) );
    
    myVcodec = "libx264 -preset veryfast";
  }  

  // https://trac.ffmpeg.org/wiki/Encode/H.265
  if ( myVcodec == "libx265" )
  {
    // Number of pixels must be divisible by two
    int intRecordX = getRecordWidth().toInt();
    if ( ( intRecordX % 2 ) == 1 )
      setRecordWidth( QString().number( --intRecordX ) );

    // Number of pixels must be divisible by two
    int intRecordY = getRecordHeight().toInt();
    if ( ( intRecordY % 2 ) == 1 )
      setRecordHeight( QString().number( --intRecordY ) );
    
    myVcodec = "libx265 -preset ultrafast -x265-params crf=28";
  }  
  
  nameInMoviesLocation = NameInMoviesLocation();

  ffmpegString = recordApplikation + " "
               + "-report" + " "
               + "-f x11grab" + " "
               + noMouse() + " "
               + framerate + " "
               + "-video_size" + " " + getRecordWidth() + "x" + getRecordHeight() + " "
               + "-i " + DISPLAY + "+" + deltaX + "," + deltaY + " "
               + myAlsa() + " "
               + "-pix_fmt yuv420p" + " "
               + "-c:v" + " " + myVcodec + " "
               + myAcodec() + " "
               + "-q:v 1" + " "
               + "-s" + " " + getRecordWidth() + "x" + getRecordHeight() + " "
               + "-f" + " " + myUi.VideoContainerComboBox->itemData( myUi.VideoContainerComboBox->currentIndex() ).toString() + " ";
  
  startRecord( PathTempLocation() + QDir::separator() + nameInMoviesLocation );
  
  QFile FileVokoscreenLog(settingsPath.absolutePath() + QDir::separator() + vkSettings.getProgName() + ".log");
  if ( !FileVokoscreenLog.open( QIODevice::WriteOnly ) ) 
    qDebug() << "Datei konnte nicht angelegt werden: " << FileVokoscreenLog.errorString();
  
  QTextStream stream( &FileVokoscreenLog );
  stream << vkSettings.getProgName() << " Version: " << vkSettings.getVersion() << "\n";
  stream << "Record resolution: " << getRecordWidth() << "x" << getRecordHeight() << "\n";
  stream << "Alsa string: " << myAlsa() << "\n";
  stream << "Qt Version: " << qVersion() << "\n";
  stream << "ffmpeg Version: " << getFfmpegVersion() << "\n";
  stream << "Record String: " << ffmpegString << PathMoviesLocation() << QDir::separator() << nameInMoviesLocation << "\n";
}


void screencast::startRecord( QString RecordPathName )
{
  qDebug() << "[vokoscreen]"<< "Executive command :" << ffmpegString + RecordPathName;
  qDebug( " " );
  
  if ( myUi.PulseDeviceRadioButton->isChecked() )
  {
    QProcess Process;
//    QString value = QvkPulse::myPulseDevice( Pulseframe );// Original
    QString value = QvkPulse::myPulseDevice( myUi.verticalLayout_3 );
    if ( value == "vokoscreenMix.monitor" )
    {
      Process.start("pactl load-module module-null-sink sink_name=vokoscreenMix");
      Process.waitForFinished();
      QString modulNumber = Process.readAllStandardOutput();
      Process.close();
      qDebug();
      qDebug() << "[vokoscreen] ---Begin Pulse loade module---";
      modulNumber.replace("\n", "");    
      qDebug() << "[vokoscreen] pactl load-module module-null-sink sink_name=vokoscreenMix " << modulNumber;
    
//      QList<QCheckBox *> listQFrame = Pulseframe->findChildren<QCheckBox *>();// Original
      QList<QCheckBox *> listQFrame = myUi.scrollAreaWidgetContents->findChildren<QCheckBox *>();
      QCheckBox *box;
      QList<int> integerList;
      for ( int i = 0; i < listQFrame.count(); i++ )
      {
        box = listQFrame.at( i );
        if ( box->checkState() == Qt::Checked  )
          integerList.append( i );
      }
      
      for ( int i = 0; i < integerList.count(); i++ )
      {
        box = listQFrame[ integerList[ i ] ];
        Process.start("pactl load-module module-loopback source=" + box->accessibleName() + " sink=vokoscreenMix");
        Process.waitForFinished();
        QString modulNumber = Process.readAllStandardOutput();
        modulNumber.replace("\n", "");
        Process.close();
        qDebug() << "[vokoscreen] pactl load-module module-loopback source=" << box->accessibleName() << "sink=vokoscreenMix number" << modulNumber;
      }
      qDebug() << "[vokoscreen] ---End Pulse loade module---";
      qDebug( " " );
    }
  
    if ( value > "" )
    {
      QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
      env.insert( "PULSE_SOURCE", value );
      SystemCall->setProcessEnvironment( env );
    }
  }

  SystemCall->start( ffmpegString + RecordPathName );

  beginTime  = QDateTime::currentDateTime();
}


void screencast::Stop()
{
  shortcutStart->setEnabled( true );
  shortcutStop->setEnabled( false );

  if ( SystemCall->state() == QProcess::Running )
  {
    SystemCall->terminate();
    SystemCall->waitForFinished();
  }

  if ( pause )
  {
    QDir dir( PathTempLocation() );
    QStringList stringList = dir.entryList(QDir::Files, QDir::Time | QDir::Reversed);
#ifdef QT4    
    QString mergeFile = QDesktopServices::storageLocation ( QDesktopServices::TempLocation ) + QDir::separator() + "mergeFile.txt";
#endif
#ifdef QT5
    QString mergeFile = QStandardPaths::writableLocation( QStandardPaths::TempLocation ) + QDir::separator() + "mergeFile.txt";
#endif
    QFile file( mergeFile );
    file.open( QIODevice::WriteOnly | QIODevice::Text );
      QString videoFiles;
      for ( int i = 0; i < stringList.size(); ++i )
      {
        videoFiles.append( "file " ).append( PathTempLocation() ).append( QDir::separator() ).append( stringList[ i ] ).append( "\n" );
#ifdef QT4	
        file.write( videoFiles.toAscii() );
#endif
#ifdef QT5
        file.write( videoFiles.toLatin1() );
#endif
        videoFiles = "";
      }
    file.close();

    QString mergeString = recordApplikation + " -report -f concat -i " + mergeFile + " -c copy " + PathMoviesLocation() + QDir::separator() + nameInMoviesLocation;
    SystemCall->start( mergeString );
    SystemCall->waitForFinished();
    
    for ( int i = 0; i < stringList.size(); ++i )
      dir.remove( PathTempLocation().append( QDir::separator() ).append( stringList.at( i ) ) );

    file.remove();

    qDebug() << "[vokoscreen]" << "Mergestring :" << mergeString;
   }
   else    
  {
    QString FileInTemp = PathTempLocation() + QDir::separator() + nameInMoviesLocation;
    QFile::copy ( FileInTemp, PathMoviesLocation() + QDir::separator() + nameInMoviesLocation );
    QFile::remove ( FileInTemp );
  }
  
  QDir dir_1;
  dir_1.rmdir( PathTempLocation() );
  
  pause = false;
  windowMoveTimer->stop();
  firststartWininfo = false;

  QvkPulse::pulseUnloadModule();
  
}
