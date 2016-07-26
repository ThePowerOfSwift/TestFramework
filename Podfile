# Uncomment this line to define a global platform for your project
platform :ios, '8.0'

use_frameworks!

target 'TestFramework' do

	pod 'AFNetworking', '~> 2.0'
	pod 'libextobjc', '~> 0.4'
	pod 'UIAlertView+Blocks'
	pod 'GPUImage', '~> 0.1'
	pod 'MBProgressHUD', '~> 0.9'
	pod 'PPBlinkOCR', '~> 2.0.0'
	pod 'OpenCV', '~> 3.0'

end

post_install do |installer_representation|
  installer_representation.pods_project.targets.each do |target|
    target.build_configurations.each do |config|
      config.build_settings['ONLY_ACTIVE_ARCH'] = 'NO'
    end
  end
end
